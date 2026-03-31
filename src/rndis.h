#ifndef RNDIS_H
#define RNDIS_H

#include <stdint.h>

// RNDIS Message Types
#define REMOTE_NDIS_PACKET_MSG          0x00000001
#define REMOTE_NDIS_INITIALIZE_MSG      0x00000002
#define REMOTE_NDIS_INITIALIZE_CMPLT    0x80000002
#define REMOTE_NDIS_QUERY_MSG           0x00000004
#define REMOTE_NDIS_QUERY_CMPLT         0x80000004
#define REMOTE_NDIS_SET_MSG             0x00000005
#define REMOTE_NDIS_SET_CMPLT           0x80000005

// RNDIS Packet Header (Encapsulates an Ethernet Frame)
typedef struct {
    uint32_t message_type;
    uint32_t message_length;
    uint32_t data_offset;
    uint32_t data_length;
    uint32_t oob_data_offset;
    uint32_t oob_data_length;
    uint32_t num_oob_data_elements;
    uint32_t per_packet_info_offset;
    uint32_t per_packet_info_length;
    uint32_t reserved_vc_handle;
    uint32_t reserved;
} __attribute__((packed)) rndis_packet_msg_t;

// RNDIS Initialization Message
typedef struct {
    uint32_t message_type;
    uint32_t message_length;
    uint32_t request_id;
    uint32_t major_version;
    uint32_t minor_version;
    uint32_t max_transfer_size;
} __attribute__((packed)) rndis_init_msg_t;

// OIDs (Object Identifiers) for querying device info
#define OID_GEN_CURRENT_PACKET_FILTER   0x0001010E
#define OID_802_3_CURRENT_ADDRESS       0x01010102

// Driver States
#define RNDIS_UNINITIALIZED 0
#define RNDIS_INITIALIZED   1
#define RNDIS_DATA_READY    2

void rndis_init();
void rndis_send_packet(void* data, uint32_t len);

#endif