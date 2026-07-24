// oracleseed.h - determinism layer for the oracle harness (plan doc S4).
//
// Compiled into the client only when ORACLE_HARNESS is defined (nmake ORACLE=1).
// All entropy sites route through these hooks; when the oracle is INACTIVE
// (normal app run) every hook returns its fallback untouched, so default
// runtime behavior is unchanged even in an ORACLE=1 build.
//
// Entropy sites covered (cites into v2.5-beta-1-modern):
//   1. app-level srand() at startup            -> OracleAppSeed()
//   2. CChatDoc ctor  m_seed = rand()          -> recorded (stream is pinned by 1)
//   3. CAvatarComplex::SetSequential           -> OracleTickSeed() replaces
//      (avatar.cpp:974 srand(GetTickCount()))     GetTickCount()
//
// Every seed actually consumed is recorded and emitted in the dump, closing
// the .ccc persistence gap (StartHistoryEntry::m_randStart is not persisted,
// histent.cpp:531) in the dump schema rather than by changing .ccc.

#ifndef ORACLE_SEED_H
#define ORACLE_SEED_H

#ifdef ORACLE_HARNESS

// Activate the determinism layer. seed pins the CRT PRNG stream (srand is
// called here); tickSeedBase replaces GetTickCount() in SetSequential with
// the deterministic sequence base, base+1, base+2, ...
void OracleSeedActivate(unsigned int seed, unsigned int tickSeedBase);
int  OracleSeedIsActive();

// Hooks (return fallback verbatim when inactive).
unsigned int OracleTickSeed(unsigned int fallback);

// Seed-usage ledger for the dump layer.
void OracleRecordSeed(const char *site, long value);
struct OracleSeedRecord { const char *site; long value; };
int  OracleSeedRecordCount();
const OracleSeedRecord *OracleSeedRecordAt(int i);

#endif // ORACLE_HARNESS

#endif // ORACLE_SEED_H
