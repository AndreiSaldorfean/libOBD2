#ifndef VCD_PULSEVIEW_H
#define VCD_PULSEVIEW_H

/*************************************************************************************************************
 * PulseView session setups (.pvs) with preconfigured UART decoders.
 *
 * A VCD file only holds waveforms; PulseView keeps its decoders in a separate "session setup" file. When
 * PulseView opens or imports 'trace.vcd' it loads 'trace.pvs' from the same directory if one exists, so
 * writing one next to the trace means the UART rows show up decoded without any clicking:
 *
 *   vcdPulseViewUart_t uart;
 *   char pvs[256];
 *
 *   VCD_PulseViewUartDefault(&uart, "line", "rx_data", 10400.0);   // RX, TX, baud; 8N1
 *   uart.name = "K-line";
 *   VCD_PulseViewSetupPath("trace.vcd", pvs, sizeof pvs);         // "trace.pvs"
 *   VCD_PulseViewWriteSetup(pvs, &uart, 1u);
 *
 * Channels are matched by the name PulseView gives the imported signal, which is the bare $var reference
 * (no scope prefix): "line", not "kline.line". Signal names are case sensitive.
 *
 * Checked against PulseView 0.4.2 / libsigrokdecode 0.5.3 (uart decoder). The format is the Qt INI file
 * PulseView itself writes with "Save session setup"; option values are serialized GVariants in host
 * byte order, so write the file on a machine with the same endianness as the one running PulseView
 * (x86 and ARM are both little endian).
 *************************************************************************************************************/

/* ================================================ INCLUDES =============================================== */
#include "vcd_types.h"
#include "vcd_uart.h"
#include "vcd_writer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================= MACROS ================================================ */
#define VCD_PV_MAX_DECODERS 16u /* decoders one setup can hold */

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
/* How the UART decoder prints the data words */
typedef enum
{
    VCD_PV_FORMAT_HEX = 0,
    VCD_PV_FORMAT_ASCII,
    VCD_PV_FORMAT_DEC,
    VCD_PV_FORMAT_OCT,
    VCD_PV_FORMAT_BIN,
} vcdPvFormat_t;

/* One UART decoder row */
typedef struct
{
    const char     *name;     /* row title in PulseView; NULL: "UART" */
    const char     *rx;       /* signal decoded as RX, NULL for none */
    const char     *tx;       /* signal decoded as TX, NULL for none (at least one of rx/tx is needed) */
    double          baud;     /* > 0 */
    uint8_t         dataBits; /* 5..9 */
    vcdUartParity_t parity;   /* MARK/SPACE map to the decoder's "one"/"zero" */
    vcdUartStop_t   stopBits;
    bool            msbFirst;
    bool            inverted; /* idle low, applies to rx and tx */
    vcdPvFormat_t   format;
    uint32_t        color;    /* row colour 0xAARRGGBB, 0: PulseView picks one */
} vcdPulseViewUart_t;

/* =============================================== MODULE API ============================================== */

/* 8N1, LSB first, not inverted, hex, default colour. 'rx'/'tx' may be NULL. */
void VCD_PulseViewUartDefault(vcdPulseViewUart_t *uart, const char *rx, const char *tx, double baud);

/* Takes data bits, parity, stop bits, bit order and inversion from a VCDio UART config, so the decoder
 * matches what VCD_UartWriteFrame() drew. The baud rate is cfg->bitTicks converted back with 'ts'. */
void VCD_PulseViewUartFromConfig(
    vcdPulseViewUart_t *uart,
    const vcdUartConfig_t *cfg,
    vcdTimescale_t ts,
    const char *rx,
    const char *tx);

/* Setup path for a trace: 'vcdPath' with its extension replaced by ".pvs" ("a/b.vcd" -> "a/b.pvs", "a/b"
 * -> "a/b.pvs"), which is where PulseView looks. Returns the length of the full path like snprintf();
 * the result is complete only when that is < outSize. */
size_t VCD_PulseViewSetupPath(const char *vcdPath, char *out, size_t outSize);

/* Writes a setup with 'count' UART decoders (1..VCD_PV_MAX_DECODERS) to 'path' (created/truncated).
 * VCD_ERR_ARG for invalid decoders (no channel, bad baud/data bits, names with control characters). */
vcdStatus_t VCD_PulseViewWriteSetup(const char *path, const vcdPulseViewUart_t *uarts, size_t count);

/* Same, streamed through a sink (see vcd_writer.h). */
vcdStatus_t VCD_PulseViewWriteSetupSink(
    vcdSinkFn_t sink,
    void *ctx,
    const vcdPulseViewUart_t *uarts,
    size_t count);

#ifdef __cplusplus
}
#endif

#endif /* VCD_PULSEVIEW_H */
