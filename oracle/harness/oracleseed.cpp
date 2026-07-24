// oracleseed.cpp - determinism layer implementation (see oracleseed.h).

#ifdef ORACLE_HARNESS

#include "oracleseed.h"

#include <stdlib.h>

namespace {

int g_active = 0;
unsigned int g_tickSeedBase = 0;
unsigned int g_tickSeedCount = 0;

const int MAX_SEED_RECORDS = 4096;
OracleSeedRecord g_records[MAX_SEED_RECORDS];
int g_recordCount = 0;

} // namespace

void OracleSeedActivate(unsigned int seed, unsigned int tickSeedBase) {
    g_active = 1;
    g_tickSeedBase = tickSeedBase;
    g_tickSeedCount = 0;
    g_recordCount = 0;
    srand(seed);
    OracleRecordSeed("srand.initial", (long)seed);
}

int OracleSeedIsActive() { return g_active; }

unsigned int OracleTickSeed(unsigned int fallback) {
    if (!g_active) return fallback;
    unsigned int v = g_tickSeedBase + g_tickSeedCount++;
    OracleRecordSeed("SetSequential.tick", (long)v);
    return v;
}

void OracleRecordSeed(const char *site, long value) {
    if (!g_active) return;
    if (g_recordCount >= MAX_SEED_RECORDS) return;
    g_records[g_recordCount].site = site;
    g_records[g_recordCount].value = value;
    ++g_recordCount;
}

int OracleSeedRecordCount() { return g_recordCount; }

const OracleSeedRecord *OracleSeedRecordAt(int i) {
    return (i >= 0 && i < g_recordCount) ? &g_records[i] : 0;
}

#endif // ORACLE_HARNESS
