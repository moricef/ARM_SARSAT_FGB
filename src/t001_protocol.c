#include "t001_protocol.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

// =============================
// BCH Functions (T.001 Annex B Compliant)
// =============================

uint32_t compute_bch(uint64_t data, int num_bits, uint32_t poly,
                     int poly_degree, uint32_t poly_mask) {
    uint32_t reg = 0;
    uint32_t poly_val = poly & ((1UL << (poly_degree + 1)) - 1);

    // Process data bits
    for (int i = num_bits - 1; i >= 0; i--) {
        uint8_t bit = (data >> i) & 1;
        uint8_t msb = (reg >> (poly_degree - 1)) & 1;
        reg = ((reg << 1) | bit) & poly_mask;
        if (msb) reg ^= poly_val;
    }

    // Padding bits
    for (int i = 0; i < poly_degree; i++) {
        uint8_t msb = (reg >> (poly_degree - 1)) & 1;
        reg = (reg << 1) & poly_mask;
        if (msb) reg ^= poly_val;
    }

    return reg;
}

uint32_t compute_bch1(uint64_t data) {
    return compute_bch(data, BCH1_DATA_BITS, BCH1_POLY, BCH1_DEGREE, BCH1_POLY_MASK);
}

uint16_t compute_bch2(uint32_t data) {
    return (uint16_t)compute_bch((uint64_t)data, BCH2_DATA_BITS, BCH2_POLY,
                                  BCH2_DEGREE, BCH2_POLY_MASK);
}

// =============================
// Bit Operations
// =============================

void set_bit_field(uint8_t *frame, uint16_t cs_start_bit, uint8_t length, uint64_t value) {
    for (uint8_t i = 0; i < length; i++) {
        frame[CS_BIT(cs_start_bit + i)] = (value >> (length - 1 - i)) & 1;
    }
}

uint64_t get_bit_field(const uint8_t *frame, uint16_t cs_start_bit, uint8_t length) {
    uint64_t value = 0;
    for (uint8_t i = 0; i < length; i++) {
        value = (value << 1) | frame[CS_BIT(cs_start_bit + i)];
    }
    return value;
}

// =============================
// GPS Encoding Functions
// =============================

uint32_t compute_30min_position(double lat, double lon) {
    // Convert to 0.5° steps with rounding
    int16_t lat_steps = (int16_t)round(lat * 2.0);
    int16_t lon_steps = (int16_t)round(lon * 2.0);

    // Clamping
    lat_steps = (lat_steps < -128) ? -128 : (lat_steps > 127) ? 127 : lat_steps;
    lon_steps = (lon_steps < -512) ? -512 : (lon_steps > 511) ? 511 : lon_steps;

    // Convert to 32-bit before shifting
    uint32_t lat_code = (uint32_t)(lat_steps & 0x1FF);  // 9 bits
    uint32_t lon_code = (uint32_t)(lon_steps & 0x3FF);  // 10 bits

    return (lat_code << 10) | lon_code;
}

uint32_t compute_4sec_offset(double lat, double lon, uint32_t position_30min) {
    // Extract reference position from PDF-1 (19 bits)
    uint16_t lat_ref_raw = (position_30min >> 10) & 0x1FF;  // 9 bits latitude
    uint16_t lon_ref_raw = position_30min & 0x3FF;          // 10 bits longitude

    // Two's complement to signed
    int16_t lat_ref_signed = lat_ref_raw;
    int16_t lon_ref_signed = lon_ref_raw;

    // Sign extension for two's complement
    if (lat_ref_raw & 0x100) lat_ref_signed |= 0xFE00;
    if (lon_ref_raw & 0x200) lon_ref_signed |= 0xFC00;

    // Reference position in degrees (0.5° resolution)
    double lat_ref_deg = lat_ref_signed * 0.5;
    double lon_ref_deg = lon_ref_signed * 0.5;

    // Calculate offsets in degrees
    double lat_offset_deg = lat - lat_ref_deg;
    double lon_offset_deg = lon - lon_ref_deg;

    // Convert to minutes
    double lat_offset_min = fabs(lat_offset_deg) * 60.0;
    double lon_offset_min = fabs(lon_offset_deg) * 60.0;

    // Sign bits (1 = positive, 0 = negative)
    uint8_t lat_sign = (lat_offset_deg >= 0) ? 1 : 0;
    uint8_t lon_sign = (lon_offset_deg >= 0) ? 1 : 0;

    // Integer minutes and seconds
    uint8_t lat_min_int = (uint8_t)lat_offset_min;
    uint8_t lon_min_int = (uint8_t)lon_offset_min;

    double lat_sec = (lat_offset_min - lat_min_int) * 60.0;
    double lon_sec = (lon_offset_min - lon_min_int) * 60.0;

    // 4-second increments with rounding
    uint8_t lat_sec_4 = (uint8_t)((lat_sec + 2.0) / 4.0);
    uint8_t lon_sec_4 = (uint8_t)((lon_sec + 2.0) / 4.0);

    // Saturation to 4 bits max
    if (lat_min_int > 15) lat_min_int = 15;
    if (lon_min_int > 15) lon_min_int = 15;
    if (lat_sec_4 > 15) lat_sec_4 = 15;
    if (lon_sec_4 > 15) lon_sec_4 = 15;

    // Encode latitude (9 bits): sign(1) + minutes(4) + 4sec(4)
    uint16_t lat_encoded = ((lat_sign & 0x1) << 8) | ((lat_min_int & 0xF) << 4) | (lat_sec_4 & 0xF);

    // Encode longitude (9 bits): sign(1) + minutes(4) + 4sec(4)
    uint16_t lon_encoded = ((lon_sign & 0x1) << 8) | ((lon_min_int & 0xF) << 4) | (lon_sec_4 & 0xF);

    // Final assembly (18 bits): latitude(9) + longitude(9)
    return (((uint32_t)lat_encoded << 9) | lon_encoded) & 0x3FFFF;
}

gps_position_t encode_gps_position(double lat, double lon) {
    gps_position_t result = {0};

    if (fabs(lat) > 90.0 || fabs(lon) > 180.0) {
        return result;
    }

    // 40-bit encoding (not used in T.001 short messages)
    uint32_t lat_units = (uint32_t)round(fabs(lat) * 900.0) & 0x7FFFF;
    uint32_t lon_units = (uint32_t)round(fabs(lon) * 900.0) & 0x7FFFF;

    uint64_t lat_encoded = ((uint64_t)lat_units << 1) | (lat < 0.0 ? 1ULL : 0ULL);
    uint64_t lon_encoded = ((uint64_t)lon_units << 1) | (lon < 0.0 ? 1ULL : 0ULL);

    result.full_position_40bit = (lat_encoded << 20) | lon_encoded;
    result.coarse_position_21bit = (uint32_t)(result.full_position_40bit >> 19);
    result.fine_position_19bit = compute_30min_position(lat, lon);
    result.offset_position_18bit = compute_4sec_offset(lat, lon, result.fine_position_19bit);

    return result;
}

uint8_t altitude_to_code(double altitude) {
    if (altitude < 400)   return 0x0;
    if (altitude < 800)   return 0x1;
    if (altitude < 1200)  return 0x2;
    if (altitude < 1600)  return 0x3;
    if (altitude < 2200)  return 0x4;
    if (altitude < 2800)  return 0x5;
    if (altitude < 3400)  return 0x6;
    if (altitude < 4000)  return 0x7;
    if (altitude < 4800)  return 0x8;
    if (altitude < 5600)  return 0x9;
    if (altitude < 6600)  return 0xA;
    if (altitude < 7600)  return 0xB;
    if (altitude < 8800)  return 0xC;
    if (altitude < 10000) return 0xD;
    if (altitude > 10000) return 0xE;
    return 0xF;
}

// =============================
// Frame Construction (T.001 Compliant)
// =============================

void build_t001_frame(uint8_t *frame, const beacon_config_t *config) {
    memset(frame, 0, MESSAGE_BITS);

    // Preamble (15 ones)
    set_bit_field(frame, FRAME_PREAMBLE_START, FRAME_PREAMBLE_LENGTH, 0x7FFF);

    // Sync pattern
    uint16_t sync = (config->mode == BEACON_MODE_TEST) ? SYNC_SELF_TEST : SYNC_NORMAL_LONG;
    set_bit_field(frame, FRAME_SYNC_START, FRAME_SYNC_LENGTH, sync);

    // Format and protocol flags
    set_bit_field(frame, FRAME_FORMAT_FLAG_BIT, 1, 1);  // Long message
    set_bit_field(frame, FRAME_PROTOCOL_FLAG_BIT, 1, 0);  // Location protocol

    // Country and protocol codes
    set_bit_field(frame, FRAME_COUNTRY_START, FRAME_COUNTRY_LENGTH, COUNTRY_CODE_FRANCE);
    set_bit_field(frame, FRAME_PROTOCOL_START, FRAME_PROTOCOL_LENGTH, PROTOCOL_ELT_DT);

    // Beacon ID
    set_bit_field(frame, FRAME_BEACON_ID_START, FRAME_BEACON_ID_LENGTH, config->beacon_id);

    // GPS position encoding
    gps_position_t gps_pos = encode_gps_position(config->latitude, config->longitude);
    set_bit_field(frame, FRAME_POSITION_START, FRAME_POSITION_LENGTH, gps_pos.fine_position_19bit);

    // BCH1 calculation (PDF-1: bits 25-85)
    uint64_t pdf1_data = get_bit_field(frame, 25, 61);
    uint32_t bch1 = compute_bch1(pdf1_data);
    set_bit_field(frame, FRAME_BCH1_START, FRAME_BCH1_LENGTH, bch1);

    // PDF-2 additional data
    set_bit_field(frame, FRAME_ACTIVATION_START, FRAME_ACTIVATION_LENGTH, 0x0);  // Manual activation

    uint8_t alt_code = altitude_to_code(config->altitude);
    set_bit_field(frame, FRAME_ALTITUDE_START, FRAME_ALTITUDE_LENGTH, alt_code);

    set_bit_field(frame, FRAME_FRESHNESS_START, FRAME_FRESHNESS_LENGTH, 0x2);  // <4 hours
    set_bit_field(frame, FRAME_OFFSET_START, FRAME_OFFSET_LENGTH, gps_pos.offset_position_18bit);

    // BCH2 calculation (PDF-2: bits 107-132)
    uint32_t pdf2_data = (uint32_t)get_bit_field(frame, 107, 26);
    uint16_t bch2 = compute_bch2(pdf2_data);
    set_bit_field(frame, FRAME_BCH2_START, FRAME_BCH2_LENGTH, bch2);
}

bool validate_t001_frame(const uint8_t *frame) {
    // Validate BCH1
    uint64_t pdf1 = get_bit_field(frame, 25, 61);
    uint32_t bch1_calc = compute_bch1(pdf1);
    uint32_t bch1_recv = (uint32_t)get_bit_field(frame, 86, 21);

    // Validate BCH2
    uint32_t pdf2 = (uint32_t)get_bit_field(frame, 107, 26);
    uint16_t bch2_calc = compute_bch2(pdf2);
    uint16_t bch2_recv = (uint16_t)get_bit_field(frame, 133, 12);

    return (bch1_calc == bch1_recv) && (bch2_calc == bch2_recv);
}

// =============================
// Debug Functions
// =============================

void print_frame_hex(const uint8_t *frame) {
    printf("Frame HEX (18 bytes): ");
    for (int byte = 0; byte < 18; byte++) {
        uint8_t byte_val = 0;
        for (int bit = 0; bit < 8; bit++) {
            byte_val = (byte_val << 1) | frame[byte * 8 + bit];
        }
        printf("%02X ", byte_val);
    }
    printf("\n");
}

void print_frame_analysis(const uint8_t *frame) {
    printf("\n=== T.001 Frame Analysis ===\n");
    printf("Preamble    (1-15):   0x%04lX\n", get_bit_field(frame, 1, 15));
    printf("Sync        (16-24):  0x%03lX\n", get_bit_field(frame, 16, 9));
    printf("Format Flag (25):     %ld\n", get_bit_field(frame, 25, 1));
    printf("Protocol    (26):     %ld\n", get_bit_field(frame, 26, 1));
    printf("Country     (27-36):  0x%03lX (%ld)\n",
           get_bit_field(frame, 27, 10), get_bit_field(frame, 27, 10));
    printf("Protocol    (37-40):  0x%lX\n", get_bit_field(frame, 37, 4));
    printf("Beacon ID   (41-66):  0x%07lX\n", get_bit_field(frame, 41, 26));
    printf("Position    (67-85):  0x%05lX\n", get_bit_field(frame, 67, 19));
    printf("BCH1        (86-106): 0x%06lX\n", get_bit_field(frame, 86, 21));
    printf("Activation  (107-108): 0x%lX\n", get_bit_field(frame, 107, 2));
    printf("Altitude    (109-112): 0x%lX\n", get_bit_field(frame, 109, 4));
    printf("Freshness   (113-114): 0x%lX\n", get_bit_field(frame, 113, 2));
    printf("Offset      (115-132): 0x%05lX\n", get_bit_field(frame, 115, 18));
    printf("BCH2        (133-144): 0x%03lX\n", get_bit_field(frame, 133, 12));

    // Validation
    bool valid = validate_t001_frame(frame);
    printf("\nFrame Validation: %s\n", valid ? "PASS" : "FAIL");
}
