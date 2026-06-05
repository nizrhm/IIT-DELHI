from ryu.base import app_manager
from ryu.controller import ofp_event
from ryu.controller.handler import CONFIG_DISPATCHER, MAIN_DISPATCHER, DEAD_DISPATCHER, set_ev_cls
from ryu.ofproto import ofproto_v1_3
from ryu.lib.packet import packet, ethernet, arp, ether_types, lldp
from ryu.lib import hub
import time
from collections import defaultdict


class AdaptiveLatencyRouter(app_manager.RyuApp):
    OFP_VERSIONS = [ofproto_v1_3.OFP_VERSION]

    # Time between beacons per switch (seconds)
    BEACON_EMISSION_GAP = 2
    # Maximum age before a link is considered dead (seconds)
    STALE_LINK_THRESHOLD = 6

    def __init__(self, *args, **kwargs):
        super(AdaptiveLatencyRouter, self).__init__(*args, **kwargs)
        
        # Per-switch state
        self.datapath_references = {}
        self.switch_port_catalog = defaultdict(dict)
        self.switch_beacon_tasks = {}
        
        # End-host registry
        self.endpoint_registry = {}
        self.arp_resolution_cache = {}
        
        # Network graph
        self.adjacency_matrix = defaultdict(dict)
        self.link_metrics = defaultdict(lambda: defaultdict(lambda: 1e9))
        self.shortest_path_cache = {}
        
        # Broadcast deduplication
        self.observed_flood_signatures = set()

    # ---------------------- SWITCH LIFECYCLE ----------------------
    @set_ev_cls(ofp_event.EventOFPStateChange, [MAIN_DISPATCHER, DEAD_DISPATCHER])
    def _react_to_switch_state_transition(self, switch_status):
        switch_datapath = switch_status.datapath
        switch_unique_id = switch_datapath.id

        if switch_status.state == MAIN_DISPATCHER:
            self.datapath_references[switch_unique_id] = switch_datapath
            self._initiate_port_discovery(switch_datapath)
            self._schedule_beacon_emission(switch_unique_id)
        elif switch_status.state == DEAD_DISPATCHER:
            self._terminate_switch_resources(switch_unique_id)

    def _initiate_port_discovery(self, target_switch):
        request = target_switch.ofproto_parser.OFPPortDescStatsRequest(target_switch)
        target_switch.send_msg(request)

    @set_ev_cls(ofp_event.EventOFPPortDescStatsReply, MAIN_DISPATCHER)
    def _record_switch_ports(self, port_reply_event):
        switch_id = port_reply_event.msg.datapath.id
        self.switch_port_catalog[switch_id] = {
            port.port_no: port for port in port_reply_event.msg.body
        }

    @set_ev_cls(ofp_event.EventOFPSwitchFeatures, CONFIG_DISPATCHER)
    def _install_default_pipeline(self, feature_event):
        datapath = feature_event.msg.datapath
        switch_id = datapath.id
        self.datapath_references[switch_id] = datapath

        ofp = datapath.ofproto
        parser = datapath.ofproto_parser

        # Send all unmatched packets to controller
        table_miss = parser.OFPFlowMod(
            datapath=datapath,
            priority=0,
            match=parser.OFPMatch(),
            instructions=[
                parser.OFPInstructionActions(
                    ofp.OFPIT_APPLY_ACTIONS,
                    [parser.OFPActionOutput(ofp.OFPP_CONTROLLER, ofp.OFPCML_NO_BUFFER)]
                )
            ]
        )
        datapath.send_msg(table_miss)
        self._initiate_port_discovery(datapath)

    def _schedule_beacon_emission(self, switch_identifier):
        """Launch a dedicated beacon task for a single switch."""
        if switch_identifier in self.switch_beacon_tasks:
            hub.kill(self.switch_beacon_tasks[switch_identifier])
        task = hub.spawn(self._beacon_emitter_for_switch, switch_identifier)
        self.switch_beacon_tasks[switch_identifier] = task

    def _beacon_emitter_for_switch(self, switch_id):
        """Emit beacons only for one switch until it disconnects."""
        while switch_id in self.datapath_references:
            current_time = time.time()
            dp = self.datapath_references[switch_id]
            ports = self.switch_port_catalog.get(switch_id, {})
            
            for port_number in ports:
                if port_number > dp.ofproto.OFPP_MAX or port_number == dp.ofproto.OFPP_LOCAL:
                    continue
                self._transmit_single_beacon(dp, port_number, current_time)
                
            hub.sleep(self.BEACON_EMISSION_GAP)

    def _transmit_single_beacon(self, datapath, output_port, emission_timestamp):
        beacon_id = f"alr://{datapath.id:016x}".encode()
        port_id = str(output_port).encode()
        timestamp_data = str(emission_timestamp).encode()

        lldp_payload = lldp.lldp([
            lldp.ChassisID(subtype=lldp.ChassisID.SUB_LOCALLY_ASSIGNED, chassis_id=beacon_id),
            lldp.PortID(subtype=lldp.PortID.SUB_PORT_COMPONENT, port_id=port_id),
            lldp.TTL(ttl=self.STALE_LINK_THRESHOLD),
            lldp.SystemDescription(system_description=timestamp_data),
            lldp.End()
        ])

        eth_frame = ethernet.ethernet(
            dst=lldp.LLDP_MAC_NEAREST_BRIDGE,
            src=datapath.ports[output_port].hw_addr,
            ethertype=ether_types.ETH_TYPE_LLDP
        )

        pkt = packet.Packet()
        pkt.add_protocol(eth_frame)
        pkt.add_protocol(lldp_payload)
        pkt.serialize()

        out = datapath.ofproto_parser.OFPPacketOut(
            datapath=datapath,
            buffer_id=datapath.ofproto.OFP_NO_BUFFER,
            in_port=datapath.ofproto.OFPP_CONTROLLER,
            actions=[datapath.ofproto_parser.OFPActionOutput(output_port)],
            data=pkt.data
        )
        datapath.send_msg(out)

    def _terminate_switch_resources(self, switch_id):
        """Clean up all resources associated with a disconnected switch."""
        self.datapath_references.pop(switch_id, None)
        self.switch_port_catalog.pop(switch_id, None)
        self.switch_beacon_tasks.pop(switch_id, None)
        
        # Remove from topology
        self.adjacency_matrix.pop(switch_id, None)
        self.link_metrics.pop(switch_id, None)
        for neighbor in list(self.adjacency_matrix.keys()):
            self.adjacency_matrix[neighbor].pop(switch_id, None)
            self.link_metrics[neighbor].pop(switch_id, None)
            
        # Invalidate routes
        expired_paths = [k for k in self.shortest_path_cache if switch_id in k]
        for key in expired_paths:
            del self.shortest_path_cache[key]

    # ---------------------- PACKET PROCESSING ----------------------
    @set_ev_cls(ofp_event.EventOFPPacketIn, MAIN_DISPATCHER)
    def _process_incoming_packet(self, packet_in):
        msg = packet_in.msg
        dpid = msg.datapath.id
        ingress = msg.match['in_port']
        raw = msg.data

        pkt = packet.Packet(raw)
        eth = pkt.get_protocol(ethernet.ethernet)
        if not eth:
            return

        src_mac = eth.src
        dst_mac = eth.dst

        # Always learn source
        self.endpoint_registry[src_mac] = (dpid, ingress)

        eth_type = eth.ethertype
        if eth_type == ether_types.ETH_TYPE_LLDP:
            lldp_pkt = pkt.get_protocol(lldp.lldp)
            if lldp_pkt:
                self._process_topology_beacon(msg, lldp_pkt)
        elif eth_type == ether_types.ETH_TYPE_ARP:
            self._handle_arp_packet(msg, pkt, src_mac, dst_mac, dpid, ingress)
        else:
            self._forward_data_traffic(msg, src_mac, dst_mac)

    def _process_topology_beacon(self, message, lldp_content):
        try:
            chassis = lldp_content.tlvs[0].chassis_id.decode()
            if not chassis.startswith('alr://'):
                return
            remote_switch = int(chassis[6:], 16)
            remote_port_num = int(lldp_content.tlvs[1].port_id.decode())
            sent_at = float(lldp_content.tlvs[3].system_description.decode())
        except Exception:
            return

        local_switch = message.datapath.id
        local_port = message.match['in_port']
        one_way_delay = (time.time() - sent_at) / 2.0

        # Update bidirectional adjacency
        self.adjacency_matrix[remote_switch][local_switch] = (remote_port_num, local_port)
        self.adjacency_matrix[local_switch][remote_switch] = (local_port, remote_port_num)
        self.link_metrics[remote_switch][local_switch] = one_way_delay
        self.link_metrics[local_switch][remote_switch] = one_way_delay

        # Clear cached paths
        for cache_key in list(self.shortest_path_cache.keys()):
            if local_switch in cache_key or remote_switch in cache_key:
                del self.shortest_path_cache[cache_key]

    def _handle_arp_packet(self, msg, full_pkt, src, dst, switch_id, input_port):
        arp_pkt = full_pkt.get_protocol(arp.arp)
        if not arp_pkt:
            return

        sender_ip = arp_pkt.src_ip
        target_ip = arp_pkt.dst_ip
        self.arp_resolution_cache[sender_ip] = (src, switch_id, input_port)

        if arp_pkt.opcode == arp.ARP_REQUEST:
            if target_ip in self.arp_resolution_cache:
                self._reply_to_arp_query(msg.datapath, input_port, src, sender_ip, target_ip)
                self._establish_flows_for_pair(src, sender_ip, target_ip)
            else:
                self._flood_arp_request(msg, input_port)

    def _reply_to_arp_query(self, datapath, port, requester_mac, requester_ip, target_ip):
        target_mac, _, _ = self.arp_resolution_cache[target_ip]
        eth_reply = ethernet.ethernet(
            src=target_mac,
            dst=requester_mac,
            ethertype=ether_types.ETH_TYPE_ARP
        )
        arp_reply = arp.arp(
            opcode=arp.ARP_REPLY,
            src_mac=target_mac,
            src_ip=target_ip,
            dst_mac=requester_mac,
            dst_ip=requester_ip
        )
        reply_pkt = packet.Packet()
        reply_pkt.add_protocol(eth_reply)
        reply_pkt.add_protocol(arp_reply)
        reply_pkt.serialize()

        parser = datapath.ofproto_parser
        out = parser.OFPPacketOut(
            datapath=datapath,
            buffer_id=datapath.ofproto.OFP_NO_BUFFER,
            in_port=datapath.ofproto.OFPP_CONTROLLER,
            actions=[parser.OFPActionOutput(port)],
            data=reply_pkt.data
        )
        datapath.send_msg(out)

    def _establish_flows_for_pair(self, mac_a, ip_a, ip_b):
        if ip_b not in self.arp_resolution_cache:
            return
        mac_b, dpid_b, _ = self.arp_resolution_cache[ip_b]
        dpid_a = self.endpoint_registry[mac_a][0]

        path_ab = self._compute_shortest_path(dpid_a, dpid_b)
        path_ba = self._compute_shortest_path(dpid_b, dpid_a)

        if path_ab:
            self._install_path_flows(path_ab, mac_a, mac_b)
        if path_ba:
            self._install_path_flows(path_ba, mac_b, mac_a)

    def _forward_data_traffic(self, msg, src, dst):
        dp = msg.datapath
        dpid = dp.id
        in_port = msg.match['in_port']

        if dst in self.endpoint_registry:
            dst_dpid, dst_port = self.endpoint_registry[dst]
            if dpid == dst_dpid:
                out_port = dst_port
            else:
                path = self._compute_shortest_path(dpid, dst_dpid)
                if not path:
                    self._controlled_flood(msg, in_port)
                    return
                self._install_path_flows(path, src, dst)
                out_port = self._get_path_exit(dpid, path)
        else:
            self._controlled_flood(msg, in_port)
            return

        parser = dp.ofproto_parser
        actions = [parser.OFPActionOutput(out_port)]
        out = parser.OFPPacketOut(
            datapath=dp,
            buffer_id=msg.buffer_id,
            in_port=in_port,
            actions=actions,
            data=msg.data if msg.buffer_id == dp.ofproto.OFP_NO_BUFFER else None
        )
        dp.send_msg(out)

    def _controlled_flood(self, msg, in_port):
        pkt = packet.Packet(msg.data)
        eth = pkt.get_protocol(ethernet.ethernet)
        arp_pkt = pkt.get_protocol(arp.arp)

        signature = (
            eth.ethertype,
            arp_pkt.src_ip if arp_pkt else eth.src,
            arp_pkt.dst_ip if arp_pkt else eth.dst
        )

        if signature in self.observed_flood_signatures:
            return
        self.observed_flood_signatures.add(signature)

        dp = msg.datapath
        ofp = dp.ofproto
        parser = dp.ofproto_parser

        flood_ports = [
            p for p in self.switch_port_catalog[dp.id]
            if p != in_port and p <= ofp.OFPP_MAX
        ]
        if not flood_ports:
            return

        actions = [parser.OFPActionOutput(p) for p in flood_ports]
        flood = parser.OFPPacketOut(
            datapath=dp,
            buffer_id=msg.buffer_id,
            in_port=in_port,
            actions=actions,
            data=msg.data if msg.buffer_id == ofp.OFP_NO_BUFFER else None
        )
        dp.send_msg(flood)

    def _flood_arp_request(self, msg, in_port):
        self._controlled_flood(msg, in_port)

    # ---------------------- PATH COMPUTATION ----------------------
    def _compute_shortest_path(self, origin, destination):
        if origin == destination:
            return [origin]
        if (origin, destination) in self.shortest_path_cache:
            return self.shortest_path_cache[(origin, destination)]

        distances = defaultdict(lambda: float('inf'))
        previous = {}
        distances[origin] = 0
        unvisited = {origin}
        visited = set()

        while unvisited:
            current = min(unvisited, key=lambda x: distances[x])
            unvisited.discard(current)
            visited.add(current)
            if current == destination:
                break
            for neighbor in self.adjacency_matrix[current]:
                if neighbor in visited:
                    continue
                new_dist = distances[current] + self.link_metrics[current][neighbor]
                if new_dist < distances[neighbor]:
                    distances[neighbor] = new_dist
                    previous[neighbor] = current
                    unvisited.add(neighbor)

        if destination not in previous:
            return None

        path = []
        node = destination
        while node != origin:
            path.append(node)
            node = previous[node]
        path.append(origin)
        path.reverse()
        self.shortest_path_cache[(origin, destination)] = path
        return path

    def _get_path_exit(self, current_switch, route):
        idx = route.index(current_switch)
        if idx == len(route) - 1:
            terminal_mac = route[-1]
            return self.endpoint_registry.get(terminal_mac, (None, 1))[1]
        next_switch = route[idx + 1]
        return self.adjacency_matrix[current_switch][next_switch][0]

    def _install_path_flows(self, path, source_mac, dest_mac):
        for i, dpid in enumerate(path):
            if dpid not in self.datapath_references:
                continue
            dp = self.datapath_references[dpid]
            parser = dp.ofproto_parser

            if i == len(path) - 1:
                if dest_mac in self.endpoint_registry:
                    out_port = self.endpoint_registry[dest_mac][1]
                else:
                    continue
            else:
                next_dpid = path[i + 1]
                if next_dpid not in self.adjacency_matrix[dpid]:
                    continue
                out_port = self.adjacency_matrix[dpid][next_dpid][0]

            match = parser.OFPMatch(eth_src=source_mac, eth_dst=dest_mac)
            actions = [parser.OFPActionOutput(out_port)]
            inst = [parser.OFPInstructionActions(dp.ofproto.OFPIT_APPLY_ACTIONS, actions)]
            flow_mod = parser.OFPFlowMod(
                datapath=dp,
                priority=100,
                match=match,
                instructions=inst
            )
            dp.send_msg(flow_mod)