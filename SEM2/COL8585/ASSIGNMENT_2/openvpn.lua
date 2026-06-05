local openvpn_proto = Proto("irondome_unique_openvpn", "IronVPN Custom Protocol")

-- ---- Fields ----
local f_opcode      = ProtoField.uint8 ("irondome.opcode",      "Opcode",       base.DEC)
local f_key_id      = ProtoField.uint8 ("irondome.key_id",      "Key ID",       base.DEC)
local f_msg_type    = ProtoField.string("irondome.msg_type",    "Message Type")
local f_session_id  = ProtoField.bytes ("irondome.session_id",  "Session ID")
local f_ack_count   = ProtoField.uint8 ("irondome.ack_count",   "ACK Count",    base.DEC)
local f_packet_id   = ProtoField.uint32("irondome.packet_id",   "Packet ID",    base.DEC)
local f_payload     = ProtoField.bytes ("irondome.payload",     "Payload")

openvpn_proto.fields = {
    f_opcode, f_key_id, f_msg_type,
    f_session_id, f_ack_count,
    f_packet_id, f_payload
}

-- ---- Opcode Map ----
local opcodes = {
    [1] = "HARD_RESET_CLIENT_V1",
    [2] = "HARD_RESET_SERVER_V1",
    [3] = "SOFT_RESET_V1",
    [4] = "CONTROL_V1",
    [5] = "ACK_V1",
    [6] = "DATA_V1",
    [7] = "HARD_RESET_CLIENT_V2",
    [8] = "HARD_RESET_SERVER_V2",
    [9] = "DATA_V2",
}

-- ---- Detection Logic ----
local function is_openvpn(buf)
    if buf:len() < 2 then return false end

    local first  = buf(0,1):uint()
    local opcode = bit.rshift(first, 3)

    if opcode < 1 or opcode > 9 then return false end

    -- DATA packets
    if opcode == 6 or opcode == 9 then
        return true
    end

    -- CONTROL packets need session ID
    if buf:len() < 9 then return false end

    -- Avoid all-zero session ID
    local all_zero = true
    for i = 1, 8 do
        if buf(i,1):uint() ~= 0 then
            all_zero = false
            break
        end
    end

    return not all_zero
end

-- ---- Dissector ----
function openvpn_proto.dissector(buf, pinfo, tree)
    if not is_openvpn(buf) then return 0 end

    local first    = buf(0,1):uint()
    local opcode   = bit.rshift(first, 3)
    local key_id   = bit.band(first, 0x07)
    local msg_name = opcodes[opcode] or "UNKNOWN"

    -- Columns (IMPORTANT)
    pinfo.cols.protocol = "IronVPN"
    pinfo.cols.info = string.format("OpenVPN | %s | key=%d | len=%d", msg_name, key_id, buf:len())

    local subtree = tree:add(openvpn_proto, buf(), "IronVPN Protocol")

    subtree:add(f_opcode, buf(0,1), opcode)
    subtree:add(f_key_id, buf(0,1), key_id)
    subtree:add(f_msg_type, buf(0,1), msg_name)

    -- DATA packets
    if opcode == 6 or opcode == 9 then
        if buf:len() > 1 then
            subtree:add(f_payload, buf(1))
        end
        return buf:len()
    end

    -- CONTROL packets
    subtree:add(f_session_id, buf(1,8))
    local offset = 9

    if buf:len() > offset then
        local ack_count = buf(offset,1):uint()
        subtree:add(f_ack_count, buf(offset,1), ack_count)
        offset = offset + 1

        if buf:len() >= offset + 4 then
            subtree:add(f_packet_id, buf(offset,4))
        end
    end

    return buf:len()
end

-- ---- Heuristic Registration ----
local function heuristic_udp(buf, pinfo, tree)
    if not is_openvpn(buf) then return false end
    openvpn_proto.dissector(buf, pinfo, tree)
    return true
end

local function heuristic_tcp(buf, pinfo, tree)
    if buf:len() < 4 then return false end
    local inner = buf(2):tvb()
    if not is_openvpn(inner) then return false end
    openvpn_proto.dissector(inner, pinfo, tree)
    return true
end

openvpn_proto:register_heuristic("udp", heuristic_udp)
openvpn_proto:register_heuristic("tcp", heuristic_tcp)

print("[IronDome] Custom OpenVPN dissector loaded successfully")
