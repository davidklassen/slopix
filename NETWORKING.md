# Networking Implementation Plan

## Goals

1. **HTTP client** - fetch web pages (wget/curl-like)
2. **HTTP server** - serve HTTP requests

## Approach

Write everything from scratch through UDP. After UDP works, decide whether to continue with TCP from scratch or port from microps/level-ip.

## QEMU Setup

```bash
qemu-system-aarch64 ... \
    -device virtio-net-device,netdev=net0 \
    -netdev user,id=net0,hostfwd=tcp::8080-:80 \
    -object filter-dump,id=f1,netdev=net0,file=packets.pcap
```

| Setting | Value |
|---------|-------|
| Guest IP | 10.0.2.15 (static) |
| Gateway | 10.0.2.2 |
| DNS | 10.0.2.3 |

Port forwarding: host 8080 → guest 80 (for testing HTTP server with `curl localhost:8080`).

## virtio-net Driver

Device ID 1, MMIO transport (same as virtio-blk).

| Aspect | virtio-blk | virtio-net |
|--------|------------|------------|
| Device ID | 2 | 1 |
| Queues | 1 (request) | 2 (RX=0, TX=1) |
| Data unit | Fixed 512-byte sectors | Variable ethernet frames |
| Header | `virtio_blk_outhdr` | `virtio_net_hdr` (12 bytes, all zeros for minimal impl) |

RX queue must be pre-populated with empty buffers before DRIVER_OK. MAC address at config offset 0x100.

## Protocol Headers

```c
struct eth_hdr {                    // 14 bytes
    uint8_t  dst[6];
    uint8_t  src[6];
    uint16_t type;                  // 0x0800=IPv4, 0x0806=ARP
};

struct arp_hdr {                    // 28 bytes
    uint16_t htype;                 // 1 = Ethernet
    uint16_t ptype;                 // 0x0800 = IPv4
    uint8_t  hlen;                  // 6
    uint8_t  plen;                  // 4
    uint16_t op;                    // 1=request, 2=reply
    uint8_t  sha[6], spa[4];        // sender hw/proto addr
    uint8_t  tha[6], tpa[4];        // target hw/proto addr
};

struct ip_hdr {                     // 20 bytes
    uint8_t  ver_ihl;               // 0x45 for IPv4, no options
    uint8_t  tos;
    uint16_t len;
    uint16_t id;
    uint16_t frag;
    uint8_t  ttl;
    uint8_t  proto;                 // 1=ICMP, 6=TCP, 17=UDP
    uint16_t checksum;
    uint32_t src, dst;
};

struct icmp_hdr {                   // 8 bytes + data
    uint8_t  type;                  // 8=request, 0=reply
    uint8_t  code;
    uint16_t checksum;
    uint16_t id, seq;
};

struct udp_hdr {                    // 8 bytes
    uint16_t sport, dport;
    uint16_t len;
    uint16_t checksum;              // can be 0
};

struct tcp_hdr {                    // 20 bytes
    uint16_t sport, dport;
    uint32_t seq, ack;
    uint8_t  off;                   // (hdr_len/4) << 4
    uint8_t  flags;                 // SYN=0x02, ACK=0x10, FIN=0x01
    uint16_t win;
    uint16_t checksum;
    uint16_t urgent;
};
```

## Implementation Phases

**Phase 1: virtio-net driver**
- Scan MMIO for device ID 1, init RX/TX queues, read MAC
- `net_send()`, `net_recv()`
- Test: capture packets with Wireshark

**Phase 2: Ethernet + ARP**
- Frame parsing, ARP table, request/reply
- Test: `tcpdump -n arp` on host

**Phase 3: IPv4 + ICMP**
- IP header parsing/checksums, ICMP echo reply
- Test: observe ICMP in pcap (ping won't work with user-mode networking)

**Phase 4: UDP**
- Port dispatch, `udp_send()`, `udp_bind()`
- Test: netcat UDP

**Decision point:** TCP from scratch or port from reference impl?

**Phase 5: TCP** - 3-way handshake, seq/ack, basic retransmit, FIN close

**Phase 6: HTTP** - HTTP/1.0 client and server

## References

| Resource | Use |
|----------|-----|
| [microps](https://github.com/pandax381/microps) | Clean TCP reference (~1500 lines) |
| [level-ip](https://github.com/saminiir/level-ip) | TCP with [blog series](https://www.saminiir.com/lets-code-tcp-ip-stack-1-ethernet-arp/) |
| [VIRTIO Spec 5.1](https://docs.oasis-open.org/virtio/virtio/v1.2/virtio-v1.2.html) | Network device |
| [RFC 793](https://tools.ietf.org/html/rfc793) | TCP |
