
import time as sys_time
import collections
import hashlib
import sys # For using maxint as an equivalent to float('inf')

from ryu.base import app_manager
from ryu.controller import ofp_event
from ryu.controller.handler import CONFIG_DISPATCHER, MAIN_DISPATCHER, DEAD_DISPATCHER, set_ev_cls
from ryu.ofproto import ofproto_v1_3 as ofp_v13
from ryu.lib.packet import packet, ethernet, arp, ether_types, lldp
from ryu.lib import hub

# Constants for Optimization
class OptimizerConfig:
    PROBE_PERIOD_SEC = 1.3
    ROUTE_RECALC_SEC = 15
    ALPHA_DROP_WEIGHT = 0.5  # Weight for drop count in the cost function

# Flow Priority Levels
class FlowPriority:
    TABLE_MISS = 0
    ARP_ENTRY = 50
    DATA_PATH = 250

class ThroughputOptimizer(app_manager.RyuApp):
    OFP_VERSIONS = [ofp_v13.OFP_VERSION]

    def __init__(self, *args, **kwargs):
        super(ThroughputOptimizer, self).__init__(*args, **kwargs)
        

        self._device_state = {}           # {DPID: Datapath}
        self._port_map = collections.defaultdict(dict) # {DPID: {Port_ID: HW_Addr}}
        self._link_topology = collections.defaultdict(dict) # {DPID_A: {DPID_B: Port_A_out}}
        
        # 2. Metric and Cost State
        self._link_metrics = {
            'latency_ms': collections.defaultdict(lambda: collections.defaultdict(lambda: sys.maxsize)), # {A: {B: Delay_ms}}
            'drop_delta': collections.defaultdict(lambda: collections.defaultdict(lambda: 0)),  # {A: {B: Drop_Count_Delta}}
            'stat_history': collections.defaultdict(dict), # {DPID: {Port_ID: Last_Drop_Count}}
            'path_costs': collections.defaultdict(dict),   # {A: {B: Calculated_Cost}}
            'predecessors': collections.defaultdict(dict)  # {Source_DPID: {Dest_DPID: Previous_Node}}
        }
        
        # 3. Forwarding and Endpoint State
        self._forwarding_map = {
            'mac_location': {},            # {MAC: (DPID, Access_Port)}
            'ip_cache': {},                # {IP: (MAC, DPID, Port)}
            'active_sessions': {},         # {MAC_Pair_Tuple: Cookie_ID}
        }

        # 4. Background Tasks
        hub.spawn(self._run_probe_beaconing)
        hub.spawn(self._run_path_optimization)

    @set_ev_cls(ofp_event.EventOFPSwitchFeatures, CONFIG_DISPATCHER)
    def _switch_features_handler(self, event):
        dp = event.msg.datapath
        self._device_state[dp.id] = dp
        self._install_table_miss_flow(dp)
        self._request_port_details(dp)

    @set_ev_cls(ofp_event.EventOFPStateChange, [MAIN_DISPATCHER, DEAD_DISPATCHER])
    def _switch_state_change_handler(self, event):
        dp = event.datapath
        if event.state == DEAD_DISPATCHER:
            self._cleanup_device_state(dp.id)

    @set_ev_cls(ofp_event.EventOFPPortDescStatsReply, MAIN_DISPATCHER)
    def _port_desc_stats_reply_handler(self, event):
        dpid = event.msg.datapath.id
        for p in event.msg.body:
            self._port_map[dpid][p.port_no] = p.hw_addr

    # =========================================================================
    #  Packet-In Processing
    # =========================================================================

    @set_ev_cls(ofp_event.EventOFPPacketIn, MAIN_DISPATCHER)
    def _packet_in_handler(self, event):
        msg = event.msg
        dp = msg.datapath
        dpid = dp.id
        ingress_port = msg.match['in_port']
        
        pkt = packet.Packet(msg.data)
        eth = pkt.get_protocol(ethernet.ethernet)
        if not eth: return
            
        src_mac, dst_mac = eth.src, eth.dst
        
        # MAC Location Update
        self._forwarding_map['mac_location'][src_mac] = (dpid, ingress_port)
        
        if eth.ethertype == ether_types.ETH_TYPE_LLDP:
            self._process_lldp_frame(msg, pkt)
        elif eth.ethertype == ether_types.ETH_TYPE_ARP:
            self._process_arp_request(msg, pkt, dpid, ingress_port)
        else:
            self._handle_data_forwarding(msg, src_mac, dst_mac, ingress_port)

    # =========================================================================
    #  Optimization Engine Core
    # =========================================================================

    def _run_probe_beaconing(self):
        while True:
            for dp in self._device_state.values():
                self._send_lldp_beacon(dp)
            hub.sleep(OptimizerConfig.PROBE_PERIOD_SEC)

    def _run_path_optimization(self):
        while True:
            self._request_all_port_stats()
            hub.sleep(1.0) # Wait for replies

            self._calculate_all_link_costs()
            self._find_best_path_scores()
            self._enforce_optimized_paths()
            hub.sleep(OptimizerConfig.ROUTE_RECALC_SEC)

    def _calculate_all_link_costs(self):
        """Calculates the inverted throughput score (Cost) for each link."""
        latencies = self._link_metrics['latency_ms']
        drops = self._link_metrics['drop_delta']
        path_costs = self._link_metrics['path_costs']
        
        for src in self._link_topology:
            for dst in self._link_topology[src]:
                delay = latencies[src].get(dst, 1000)
                drop_rate = drops[src].get(dst, 0)
                
                # Inverted Score Cost = Delay + (Alpha * Drops)
                # Lower cost means higher calculated throughput score.
                cost = delay + (OptimizerConfig.ALPHA_DROP_WEIGHT * drop_rate)
                path_costs[src][dst] = cost

    def _find_best_path_scores(self):
        """Uses a Dijkstra-like approach to find the minimum cost path (Max Throughput)."""
        self._link_metrics['predecessors'].clear()
        
        nodes = list(self._link_topology.keys())
        costs = self._link_metrics['path_costs']
        
        for source in nodes:
            dist = {n: sys.maxsize for n in nodes}
            pred = {n: None for n in nodes}
            dist[source] = 0
            
            # Use a dictionary as a poor-man's priority queue for structural difference
            unvisited_dist = {source: 0} 

            while unvisited_dist:
                # Find node with minimum distance (min cost = max score)
                u = min(unvisited_dist, key=unvisited_dist.get)
                current_distance = unvisited_dist.pop(u)
                
                if current_distance > dist[u]: continue
                
                for v, weight in costs.get(u, {}).items():
                    new_dist = current_distance + weight
                    
                    if new_dist < dist[v]:
                        dist[v] = new_dist
                        pred[v] = u
                        unvisited_dist[v] = new_dist # Add/update in the queue

            self._link_metrics['predecessors'][source] = pred

    # =========================================================================
    #  Route Enforcement and Flow Management
    # =========================================================================

    def _enforce_optimized_paths(self):
        """Iterates over all active sessions and updates their flow tables."""
        
        hosts = list(self._forwarding_map['mac_location'].keys())
        
        for mac_a in hosts:
            for mac_b in hosts:
                if mac_a == mac_b: continue
                
                loc_a = self._forwarding_map['mac_location'][mac_a]
                loc_b = self._forwarding_map['mac_location'][mac_b]

                if loc_a[0] != loc_b[0]:
                    self._update_bidirectional_flows(loc_a[0], loc_b[0], mac_a, mac_b)

    def _update_bidirectional_flows(self, dpid_a, dpid_b, mac_a, mac_b):
        """Deletes old flows and installs new flows for A<->B session."""
        
        mac_pair = tuple(sorted((mac_a, mac_b)))
        
        # 1. Generate unique cookie for the session
        session_id = str(mac_pair)
        cookie = int(hashlib.sha1(session_id.encode()).hexdigest(), 16) & 0xFFFFFFFFFFFFFFFF
        
        # 2. Delete existing flows using the cookie
        self._delete_flow_session(cookie)
        path_ab = self._reconstruct_path(dpid_a, dpid_b)
        if path_ab:
            self._insert_path_segment_flows(path_ab, mac_a, mac_b, cookie)

        # 4. Path B -> A
        path_ba = self._reconstruct_path(dpid_b, dpid_a)
        if path_ba:
            self._insert_path_segment_flows(path_ba, mac_b, mac_a, cookie)
            
        self._forwarding_map['active_sessions'][mac_pair] = cookie

    def _insert_path_segment_flows(self, path, src_mac, dst_mac, cookie):
        """Installs flow entries for one direction along the computed path."""
        
        for i in range(len(path)):
            current_dpid = path[i]
            dp = self._device_state.get(current_dpid)
            if not dp: continue
            
            ofp, parser = dp.ofproto, dp.ofproto_parser
            match = parser.OFPMatch(eth_src=src_mac, eth_dst=dst_mac)
            
            if i < len(path) - 1:
 
                next_dpid = path[i+1]
                out_port = self._link_topology[current_dpid][next_dpid]
            else:
                location = self._forwarding_map['mac_location'].get(dst_mac)
                if location and location[0] == current_dpid:
                    out_port = location[1]
                else: continue # Final destination not attached here

            actions = [parser.OFPActionOutput(out_port)]
            instructions = [parser.OFPInstructionActions(ofp.OFPIT_APPLY_ACTIONS, actions)]
            mod = parser.OFPFlowMod(
                datapath=dp, priority=FlowPriority.DATA_PATH, match=match, 
                instructions=instructions, cookie=cookie,
                flags=ofp.OFPFF_SEND_FLOW_REM, command=ofp.OFPFC_ADD
            )
            dp.send_msg(mod)

    def _delete_flow_session(self, cookie_id):
        """Sends a FlowMod DELETE command for a session cookie."""
        for dp in self._device_state.values():
            ofp, parser = dp.ofproto, dp.ofproto_parser
            
            # Explicit FlowMod DELETE_STRICT
            delete_mod = parser.OFPFlowMod(
                datapath=dp, command=ofp.OFPFC_DELETE_STRICT, 
                out_port=ofp.OFPP_ANY, out_group=ofp.OFPG_ANY,
                priority=FlowPriority.DATA_PATH, match=parser.OFPMatch(),
                cookie=cookie_id, cookie_mask=0xFFFFFFFFFFFFFFFF
            )
            dp.send_msg(delete_mod)

    def _reconstruct_path(self, start_dpid, end_dpid):
        """Reconstructs the full path list from the predecessor map."""
        path = []
        pred = self._link_metrics['predecessors'].get(start_dpid)
        if not pred: return path
        
        current_node = end_dpid
        
        while current_node is not None and current_node != start_dpid:
            path.insert(0, current_node)
            current_node = pred.get(current_node)
            
        if current_node == start_dpid:
            path.insert(0, start_dpid)
            return path
        
        return []

    # =========================================================================
    #  Metric Handlers (Stats & LLDP)
    # =========================================================================
    
    @set_ev_cls(ofp_event.EventOFPPortStatsReply, MAIN_DISPATCHER)
    def _port_stats_reply_handler(self, event):
        dpid = event.msg.datapath.id
        stats_hist = self._link_metrics['stat_history']
        drop_deltas = self._link_metrics['drop_delta']
        
        for stat in event.msg.body:
            port_num = stat.port_no
            peer_dpid = next((n for n, p in self._link_topology.get(dpid, {}).items() if p == port_num), None)
            if peer_dpid is None: continue 

            if port_num not in stats_hist[dpid]:
                stats_hist[dpid][port_num] = stat.tx_dropped
                continue
                
            prev_drops = stats_hist[dpid][port_num]
            drop_delta = stat.tx_dropped - prev_drops
            stats_hist[dpid][port_num] = stat.tx_dropped
            
            drop_deltas[dpid][peer_dpid] = drop_delta

    def _request_all_port_stats(self):
        """Sends OFPPortStatsRequest to all devices."""
        for dp in self._device_state.values():
            ofp = dp.ofproto
            req = dp.ofproto_parser.OFPPortStatsRequest(dp, 0, ofp.OFPP_ANY)
            dp.send_msg(req)

    def _process_lldp_frame(self, msg, pkt):
        """Extracts topology and measures link latency."""
        try:
            lldp_data = pkt.get_protocol(lldp.lldp)
            tlvs = {type(t): t for t in lldp_data.tlvs}
            
            # Use explicit TLV access
            remote_id = int(tlvs[lldp.ChassisID].chassis_id.decode().split('//')[1], 16)
            remote_port = int(tlvs[lldp.PortID].port_id.decode())
            sent_time = float(tlvs[lldp.SystemDescription].system_description.decode())
        except Exception:
            return

        local_dpid = msg.datapath.id
        local_port = msg.match['in_port']
        latency_ms = (sys_time.time() - sent_time) * 1000.0

        # Update Topology
        self._link_topology[remote_id][local_dpid] = remote_port
        self._link_topology[local_dpid][remote_id] = local_port
        
        # Update Latency
        self._link_metrics['latency_ms'][remote_id][local_dpid] = latency_ms

    # =========================================================================
    #  General Helpers and Low-Level OpenFlow
    # =========================================================================

    def _handle_data_forwarding(self, msg, src_mac, dst_mac, in_port):
        """Forwards the initial data packet."""
        dp = msg.datapath
        dpid = dp.id
        
        dst_info = self._forwarding_map['mac_location'].get(dst_mac)
        
        if dst_info is None:
            self._send_flood_message(msg, in_port)
            return
            
        dst_dpid, dst_port = dst_info
        
        if dpid == dst_dpid:
            out_port = dst_port
        else:
            # Determine the next hop based on the pre-calculated path
            path = self._reconstruct_path(dpid, dst_dpid)
            if not path or len(path) < 2:
                self._send_flood_message(msg, in_port)
                return
            
            next_dpid = path[path.index(dpid) + 1]
            out_port = self._link_topology[dpid][next_dpid]
            
        actions = [dp.ofproto_parser.OFPActionOutput(out_port)]
        self._send_packet_out(dp, msg.buffer_id, in_port, actions, msg.data)

    def _process_arp_request(self, msg, pkt, dpid, in_port):

        arp_hdr = pkt.get_protocol(arp.arp)
        if not arp_hdr: return

        ip_src, ip_dst = arp_hdr.src_ip, arp_hdr.dst_ip
        src_mac = arp_hdr.src_mac
        
        self._forwarding_map['ip_cache'][ip_src] = (src_mac, dpid, in_port)
        
        if arp_hdr.opcode == arp.ARP_REQUEST:
            if ip_dst in self._forwarding_map['ip_cache']:
                self._send_proxy_arp_reply(msg.datapath, in_port, ip_src, ip_dst)
            else:
                self._send_flood_message(msg, in_port)

    def _send_proxy_arp_reply(self, dp, port, req_ip, target_ip):
        """Constructs and sends an ARP reply packet."""
        
        target_mac, _, _ = self._forwarding_map['ip_cache'][target_ip]
        req_mac = self._forwarding_map['ip_cache'][req_ip][0]
        
        eth_rep = ethernet.ethernet(src=target_mac, dst=req_mac, ethertype=ether_types.ETH_TYPE_ARP)
        arp_rep = arp.arp(opcode=arp.ARP_REPLY, src_mac=target_mac, src_ip=target_ip,
                           dst_mac=req_mac, dst_ip=req_ip)
                           
        reply_pkt = packet.Packet()
        reply_pkt.add_protocol(eth_rep)
        reply_pkt.add_protocol(arp_rep)
        reply_pkt.serialize()
        
        actions = [dp.ofproto_parser.OFPActionOutput(port)]
        self._send_packet_out(dp, dp.ofproto.OFP_NO_BUFFER, dp.ofproto.OFPP_CONTROLLER, actions, reply_pkt.data)

    def _send_lldp_beacon(self, dp_ref):
        dpid = dp_ref.id
        ofp = dp_ref.ofproto
        parser = dp_ref.ofproto_parser
        
        if dpid not in self._port_map: return
            
        now = sys_time.time()
        # Use a new, unique identifier format
        chassis_raw = f"thr_opt//dpid_{dpid:x}".encode() 
        time_raw = str(now).encode()
        
        common_tlvs = [
            lldp.ChassisID(subtype=lldp.ChassisID.SUB_LOCALLY_ASSIGNED, chassis_id=chassis_raw),
            lldp.TTL(ttl=5),
            lldp.SystemDescription(system_description=time_raw),
            lldp.End()
        ]
        
        for port_num, hw_addr in self._port_map[dpid].items():
            if port_num > ofp.OFPP_MAX or port_num == ofp.OFPP_LOCAL: continue
                
            port_id_tlv = lldp.PortID(subtype=lldp.PortID.SUB_PORT_COMPONENT, port_id=str(port_num).encode())
            
            beacon_pkt = packet.Packet()
            beacon_pkt.add_protocol(ethernet.ethernet(dst=lldp.LLDP_MAC_NEAREST_BRIDGE, src=hw_addr, ethertype=ether_types.ETH_TYPE_LLDP))
            beacon_pkt.add_protocol(lldp.lldp([common_tlvs[0], port_id_tlv] + common_tlvs[1:]))
            beacon_pkt.serialize()
            
            actions = [parser.OFPActionOutput(port_num)]
            self._send_packet_out(dp_ref, ofp.OFP_NO_BUFFER, ofp.OFPP_CONTROLLER, actions, beacon_pkt.data)

    def _send_packet_out(self, dp, buf_id, in_port, actions, data):
        ofp = dp.ofproto
        parser = dp.ofproto_parser
        out = parser.OFPPacketOut(datapath=dp, buffer_id=buf_id, in_port=in_port, actions=actions,
                                  data=data if buf_id == ofp.OFP_NO_BUFFER else None)
        dp.send_msg(out)

    def _send_flood_message(self, msg, in_port):
        dp = msg.datapath
        ofp = dp.ofproto
        parser = dp.ofproto_parser
        
        flood_ports = [
            p for p in self._port_map.get(dp.id, {})
            if p != in_port and p <= ofp.OFPP_MAX
        ]
        
        actions = [parser.OFPActionOutput(p) for p in flood_ports]
        self._send_packet_out(dp, msg.buffer_id, in_port, actions, msg.data)

    def _install_table_miss_flow(self, dp):
        ofp = dp.ofproto
        parser = dp.ofproto_parser
        match_all = parser.OFPMatch()
        actions = [parser.OFPActionOutput(ofp.OFPP_CONTROLLER, ofp.OFPCML_NO_BUFFER)]
        instructions = [parser.OFPInstructionActions(ofp.OFPIT_APPLY_ACTIONS, actions)]

        mod = parser.OFPFlowMod(datapath=dp, priority=FlowPriority.TABLE_MISS, match=match_all, instructions=instructions)
        dp.send_msg(mod)
        
    def _request_port_details(self, dp):
        """Requests OFPPortDescStats from the target device."""
        req = dp.ofproto_parser.OFPPortDescStatsRequest(dp, 0)
        dp.send_msg(req)

    def _cleanup_device_state(self, dpid):
        """Removes device state upon disconnection."""
        self._device_state.pop(dpid, None)
        # Clear all state entries related to the DPID
        for state_map in [self._port_map, self._link_topology]:
            state_map.pop(dpid, None)
        for metric_map in self._link_metrics.values():
             metric_map.pop(dpid, None)