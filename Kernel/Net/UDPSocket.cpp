#include <Kernel/Net/UDPSocket.h>
#include <Kernel/Net/UDP.h>
#include <Kernel/Net/NetworkAdapter.h>
#include <Kernel/Process.h>
#include <Kernel/Devices/RandomDevice.h>
#include <Kernel/Net/Routing.h>

Lockable<HashMap<word, UDPSocket*>>& UDPSocket::sockets_by_port()
{
    static Lockable<HashMap<word, UDPSocket*>>* s_map;
    if (!s_map)
        s_map = new Lockable<HashMap<word, UDPSocket*>>;
    return *s_map;
}

UDPSocketHandle UDPSocket::from_port(word port)
{
    RetainPtr<UDPSocket> socket;
    {
        LOCKER(sockets_by_port().lock());
        auto it = sockets_by_port().resource().find(port);
        if (it == sockets_by_port().resource().end())
            return { };
        socket = (*it).value;
        ASSERT(socket);
    }
    return { move(socket) };
}


UDPSocket::UDPSocket(int protocol)
    : IPv4Socket(SOCK_DGRAM, protocol)
{
}

UDPSocket::~UDPSocket()
{
    LOCKER(sockets_by_port().lock());
    sockets_by_port().resource().remove(source_port());
}

Retained<UDPSocket> UDPSocket::create(int protocol)
{
    return adopt(*new UDPSocket(protocol));
}

int UDPSocket::protocol_receive(const ByteBuffer& packet_buffer, void* buffer, size_t buffer_size, int flags, sockaddr* addr, socklen_t* addr_length)
{
    (void)flags;
    (void)addr_length;
    ASSERT(!packet_buffer.is_null());
    auto& ipv4_packet = *(const IPv4Packet*)(packet_buffer.pointer());
    auto& udp_packet = *static_cast<const UDPPacket*>(ipv4_packet.payload());
    ASSERT(udp_packet.length() >= sizeof(UDPPacket)); // FIXME: This should be rejected earlier.
    ASSERT(buffer_size >= (udp_packet.length() - sizeof(UDPPacket)));
    if (addr) {
        auto& ia = *(sockaddr_in*)addr;
        ia.sin_port = htons(udp_packet.destination_port());
    }
    memcpy(buffer, udp_packet.payload(), udp_packet.length() - sizeof(UDPPacket));
    return udp_packet.length() - sizeof(UDPPacket);
}

int UDPSocket::protocol_send(const void* data, int data_length)
{
    auto* adapter = adapter_for_route_to(destination_address());
    if (!adapter)
        return -EHOSTUNREACH;
    auto buffer = ByteBuffer::create_zeroed(sizeof(UDPPacket) + data_length);
    auto& udp_packet = *(UDPPacket*)(buffer.pointer());
    udp_packet.set_source_port(source_port());
    udp_packet.set_destination_port(destination_port());
    udp_packet.set_length(sizeof(UDPPacket) + data_length);
    memcpy(udp_packet.payload(), data, data_length);
    kprintf("sending as udp packet from %s:%u to %s:%u!\n",
        adapter->ipv4_address().to_string().characters(),
        source_port(),
        destination_address().to_string().characters(),
        destination_port());
    adapter->send_ipv4(MACAddress(), destination_address(), IPv4Protocol::UDP, move(buffer));
    return data_length;
}

KResult UDPSocket::protocol_connect(ShouldBlock)
{
    return KSuccess;
}

int UDPSocket::protocol_allocate_source_port()
{
    static const word first_ephemeral_port = 32768;
    static const word last_ephemeral_port = 60999;
    static const word ephemeral_port_range_size = last_ephemeral_port - first_ephemeral_port;
    word first_scan_port = first_ephemeral_port + (word)(RandomDevice::random_percentage() * ephemeral_port_range_size);

    LOCKER(sockets_by_port().lock());
    for (word port = first_scan_port;;) {
        auto it = sockets_by_port().resource().find(port);
        if (it == sockets_by_port().resource().end()) {
            set_source_port(port);
            sockets_by_port().resource().set(port, this);
            return port;
        }
        ++port;
        if (port > last_ephemeral_port)
            port = first_ephemeral_port;
        if (port == first_scan_port)
            break;
    }
    return -EADDRINUSE;
}
