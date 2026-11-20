#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define CB_PACKET_MAGIC 0x43424153u
#define CB_PROTOCOL_VERSION 1u
static void put_u32_be(uint8_t *out, uint32_t value)
{
    out[0]=(uint8_t)(value>>24); out[1]=(uint8_t)(value>>16);
    out[2]=(uint8_t)(value>>8); out[3]=(uint8_t)value;
}
static uint32_t get_u32_be(const uint8_t *in)
{
    return ((uint32_t)in[0]<<24)|((uint32_t)in[1]<<16)|
           ((uint32_t)in[2]<<8)|(uint32_t)in[3];
}
int main(void)
{
    uint8_t packet[12]={0};
    uint32_t seq=0x10203040u;
    put_u32_be(packet,CB_PACKET_MAGIC);
    packet[4]=CB_PROTOCOL_VERSION; packet[5]=2u;
    packet[6]=0u; packet[7]=4u; put_u32_be(packet+8,seq);
    assert(get_u32_be(packet)==CB_PACKET_MAGIC);
    assert(packet[4]==CB_PROTOCOL_VERSION && packet[5]==2u);
    assert(packet[6]==0u && packet[7]==4u);
    assert(get_u32_be(packet+8)==seq);
    assert(memcmp(packet,"CBAS",4)==0);
    puts("protocol header serialization: PASS");
    return 0;
}
