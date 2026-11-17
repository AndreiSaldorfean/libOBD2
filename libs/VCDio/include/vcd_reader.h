#ifndef VCD_READER_H
#define VCD_READER_H

/*************************************************************************************************************
 * VCD reader.
 *
 * Loads a whole .vcd file into memory and gives indexed access to every declared variable and its value
 * changes. Accepts the output of VCDio, GTKWave, Icarus Verilog, Verilator, GHDL, sigrok and the like:
 * nested scopes, aliased identifiers (one signal declared in several scopes), bit-range suffixes
 * ("data [7:0]"), multi-character identifiers, $dumpvars/$dumpoff blocks, real and string values.
 *
 * Values are returned as text without their type prefix:
 *   1-bit:  "0", "1", "x", "z" (also "u", "w", "l", "h", "-" from VHDL tools), always lower case
 *   vector: binary digits, e.g. "1010" or "x01" (lower case x/z), see VCD_ValueToU64()
 *   real:   as written, e.g. "3.25"
 *   string: as written
 *
 * Variables are addressed by index (0 .. VCD_ReaderVarCount()-1). Aliases share their change list.
 *************************************************************************************************************/

/* ================================================ INCLUDES =============================================== */
#include "vcd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================= TYPEDEFS, ENUMS, STRUCTS ======================================== */
typedef struct vcdReader vcdReader_t;

typedef struct
{
    size_t line;         /* 1-based line of the problem, 0 when not tied to a line */
    char   message[160];
} vcdParseError_t;

typedef struct
{
    const char  *name;     /* reference name incl. bit range, e.g. "data[7:0]" */
    const char  *scope;    /* dotted scope path, "" at top level */
    const char  *fullName; /* scope + "." + name */
    const char  *typeName; /* type keyword as written, e.g. "wire", "logic" */
    const char  *id;       /* identifier code */
    vcdVarType_t type;
    uint32_t     width;
} vcdVarInfo_t;

/* =============================================== MODULE API ============================================== */

/* Parses a file / a memory buffer. On failure *reader is NULL and, if 'err' is given, it describes the
 * problem. The memory buffer is not referenced after the call returns. */
vcdStatus_t VCD_ReaderOpenFile(vcdReader_t **reader, const char *path, vcdParseError_t *err);
vcdStatus_t VCD_ReaderOpenMemory(vcdReader_t **reader, const char *data, size_t len, vcdParseError_t *err);

void VCD_ReaderClose(vcdReader_t *reader);

/* Header data. Text getters return NULL when the section is absent.
 * Without a $timescale section the timescale is 1 ns. */
vcdTimescale_t VCD_ReaderTimescale(const vcdReader_t *reader);
const char *VCD_ReaderDate(const vcdReader_t *reader);
const char *VCD_ReaderVersion(const vcdReader_t *reader);
const char *VCD_ReaderComment(const vcdReader_t *reader);

/* First and last '#time' in the file (0 when there is none). */
vcdTime_t VCD_ReaderStartTime(const vcdReader_t *reader);
vcdTime_t VCD_ReaderEndTime(const vcdReader_t *reader);

size_t VCD_ReaderVarCount(const vcdReader_t *reader);

/* NULL for an invalid index. */
const vcdVarInfo_t *VCD_ReaderVar(const vcdReader_t *reader, size_t var);

/* Finds a variable by full name ("top.uart.tx"), else by its leaf name ("tx"), which may also leave out
 * the bit range ("data" finds "data[7:0]"). A leaf name matching different signals is ambiguous and
 * gives VCD_ERR_ARG; matching aliases of one signal is fine. */
vcdStatus_t VCD_ReaderFind(const vcdReader_t *reader, const char *name, size_t *var);

size_t VCD_ReaderChangeCount(const vcdReader_t *reader, size_t var);

/* The index-th change of a variable, in time order. Either output pointer may be NULL. */
vcdStatus_t VCD_ReaderChange(
    const vcdReader_t *reader,
    size_t var,
    size_t index,
    vcdTime_t *time,
    const char **value);

/* Index of the last change at or before 'time'. VCD_ERR_NOT_FOUND when the first change is later. */
vcdStatus_t VCD_ReaderFindChange(const vcdReader_t *reader, size_t var, vcdTime_t time, size_t *index);

/* Value at 'time' (the last change at or before it), NULL before the first change. */
const char *VCD_ReaderValueAt(const vcdReader_t *reader, size_t var, vcdTime_t time);

#ifdef __cplusplus
}
#endif

#endif /* VCD_READER_H */
