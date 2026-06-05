#include <sys/param.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_var.h> 
#include <net/pfil.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

static struct pfil_head *pfh_inet; 
static int dropped_count = 0; // Task Requirement: Count dropped packets

static int
block_http_hook(void *arg, struct mbuf **mp, struct ifnet *ifp, int dir, struct inpcb *inp)
{
    struct mbuf *m = *mp;
    struct ip *ip;
    struct tcphdr *tcp;
    char buffer[1024]; 
    int hlen, off, len;

    if (m == NULL || dir != PFIL_IN) return (0);

    ip = mtod(m, struct ip *);
    if (ip->ip_v != 4 || ip->ip_p != IPPROTO_TCP) return (0);

    hlen = (ip->ip_hl << 2);
    if (m->m_pkthdr.len < hlen + sizeof(struct tcphdr)) return (0);
    tcp = (struct tcphdr *)((char *)ip + hlen);
    
    if (ntohs(tcp->th_dport) == 80) {
        off = hlen + (tcp->th_off << 2);
        len = m->m_pkthdr.len - off;

        if (len > 0) {
            int copy_len = (len > sizeof(buffer) - 1) ? sizeof(buffer) - 1 : len;
            m_copydata(m, off, copy_len, buffer);
            buffer[copy_len] = '\0';

            /* Task Requirement: Block if "blocked.com" is present */
            if (strstr(buffer, "blocked.com") != NULL) {
                dropped_count++;
                
                /* Task Requirement: Print number of packets dropped and their size */
                printf("SHIELD_BLOCK: [Drop #%d] Size: %d bytes | Reason: blocked.com\n", 
                        dropped_count, m->m_pkthdr.len);

                m_freem(m);    // Drop the packet
                *mp = NULL;    // Clear the pointer
                return (EACCES); // Return error to stop processing
            }
        }
    }
    return (0); // Pass normal traffic
}

static int
shield_loader(struct module *m, int what, void *arg)
{
    switch (what) {
    case MOD_LOAD:
        printf("Platinum Shield: Firewall Fully Active.\n");
        pfh_inet = pfil_head_get(PFIL_TYPE_AF, AF_INET);
        if (pfh_inet == NULL) return (ENOENT);
        pfil_add_hook(block_http_hook, NULL, PFIL_IN | PFIL_WAITOK, pfh_inet);
        break;
    case MOD_UNLOAD:
        printf("Platinum Shield: Firewall Deactivated.\n");
        pfil_remove_hook(block_http_hook, NULL, PFIL_IN | PFIL_WAITOK, pfh_inet);
        break;
    default:
        return (EOPNOTSUPP);
    }
    return (0);
}

static moduledata_t shield_data = { "platinum_shield", shield_loader, NULL };
DECLARE_MODULE(platinum_shield, shield_data, SI_SUB_DRIVERS, SI_ORDER_MIDDLE);