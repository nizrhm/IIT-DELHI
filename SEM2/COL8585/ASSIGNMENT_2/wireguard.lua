
local wg_proto = Proto("irondome_wireguard_unique", "IronWG Custom Protocol")

local f_msg_type     = ProtoField.uint8 ("irondome_wg.msg_type",     "Message Type",   base.DEC)
local f_type_name    = ProtoField.string("irondome_wg.type_name",    "Type Name")
local f_reserved     = ProtoField.bytes ("irondome_wg.reserved",     "Reserved")
local f_sender_idx   = ProtoField.uint32("irondome_wg.sender_idx",   "Sender Index",   base.HEX)
local f_receiver_idx = ProtoField.uint32("irondome_wg.receiver_idx", "Receiver Index", base.HEX)
local f_ephemeral    = ProtoField.bytes ("irondome_wg.ephemeral",    "Ephemeral Key")
local f_static       = ProtoField.bytes ("irondome_wg.static",       "Encrypted Static")
local f_timestamp    = ProtoField.bytes ("irondome_wg.timestamp",    "Encrypted Timestamp")
local f_mac1         = ProtoField.bytes ("irondome_wg.mac1",         "MAC1")
local f_mac2         = ProtoField.bytes ("irondome_wg.mac2",         "MAC2")
local f_counter      = ProtoField.bytes ("irondome_wg.counter",      "Counter")
local f_payload      = ProtoField.bytes ("irondome_wg.payload",      "Encrypted Payload")
local f_nonce        = ProtoField.bytes ("irondome_wg.nonce",        "Nonce")
local f_cookie       = ProtoField.bytes ("irondome_wg.cookie",       "Encrypted Cookie")

wg_proto.fields = {
    f_msg_type, f_type_name, f_reserved,
    f_sender_idx, f_receiver_idx,
    f_ephemeral, f_static, f_timestamp,
    f_mac1, f_mac2, f_counter,
    f_payload, f_nonce, f_cookie
}

local wg_types = {
    [1] = "Handshake Initiation",
    [2] = "Handshake Response",
    [3] = "Cookie Reply",
    [4] = "Transport Data",
}

local wg_sizes = { [1]=148, [2]=92, [3]=64 }

-- ---- Detection ----
local function is_wireguard(buf)
    if buf:len() < 4 then return false end

    local mt = buf(0,1):uint()
    if not wg_types[mt] then return false end

    -- Reserved bytes must be zero
    if buf(1,1):uint() ~= 0 then return false end
    if buf(2,1):uint() ~= 0 then return false end
    if buf(3,1):uint() ~= 0 then return false end

    -- Size checks
    local exp = wg_sizes[mt]
    if exp and buf:len() ~= exp then return false end

    if mt == 4 and buf:len() < 16 then return false end

    return true
end

-- ---- Dissector ----
function wg_proto.dissector(buf, pinfo, tree)
    if not is_wireguard(buf) then return 0 end

    local mt        = buf(0,1):uint()
    local type_name = wg_types[mt] or "Unknown"

    pinfo.cols.protocol = "IronWireGuard"
    pinfo.cols.info = string.format("%s | len=%d", type_name, buf:len())

    local st = tree:add(wg_proto, buf(), "WireGuard Protocol (IronDome)")

    st:add(f_msg_type,  buf(0,1), mt)
    st:add(f_type_name, buf(0,1), type_name)
    st:add(f_reserved,  buf(1,3))

    if mt == 1 and buf:len() >= 148 then
        st:add(f_sender_idx, buf(4,4))
        st:add(f_ephemeral,  buf(8,32))
        st:add(f_static,     buf(40,48))
        st:add(f_timestamp,  buf(88,28))
        st:add(f_mac1,       buf(116,16))
        st:add(f_mac2,       buf(132,16))
    elseif mt == 2 and buf:len() >= 92 then
        st:add(f_sender_idx,   buf(4,4))
        st:add(f_receiver_idx, buf(8,4))
        st:add(f_ephemeral,    buf(12,32))
        st:add(f_static,       buf(44,16))
        st:add(f_mac1,         buf(60,16))
        st:add(f_mac2,         buf(76,16))
    elseif mt == 3 and buf:len() >= 64 then
        st:add(f_receiver_idx, buf(4,4))
        st:add(f_nonce,        buf(8,24))
        st:add(f_cookie,       buf(32,32))
    elseif mt == 4 and buf:len() >= 16 then
        st:add(f_receiver_idx, buf(4,4))
        st:add(f_counter,      buf(8,8))
        if buf:len() > 16 then
            st:add(f_payload, buf(16, buf:len()-16))
        end
    end

    return buf:len()
end

-- ---- Heuristic Registration ONLY (port-independent) ----
local function wg_heuristic(buf, pinfo, tree)
    if not is_wireguard(buf) then return false end
    wg_proto.dissector(buf, pinfo, tree)
    return true
end

wg_proto:register_heuristic("udp", wg_heuristic)

print("[IronDome] WireGuard dissector loaded successfully")
