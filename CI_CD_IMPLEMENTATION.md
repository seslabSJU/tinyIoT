# CI/CD Implementation for tinyIoT

## Overview
GitHub Actions workflow for automated testing of the tinyIoT server with PostgreSQL database.

## Date
2026-02-20

## Objective
Automate testing of tests/ folder scripts using GitHub Actions

## Completed Work

### 1. Fixed PostgreSQL Conditional Compilation in monitor.c

**File**: `source/server/monitor.c`

**Changes**:
- Moved `#include <libpq-fe.h>` inside `#if DB_TYPE == DB_POSTGRESQL` block
- Added conditional compilation for `extern PGconn *pg_conn` declaration
- Wrapped PostgreSQL DB reset code in conditional compilation

**Reason**:
- SQLite builds were failing because PostgreSQL headers were always included
- This allows the code to compile with either SQLite or PostgreSQL

### 2. Created GitHub Actions Workflow

**File**: `.github/workflows/test.yml`

**Features**:
- Runs on Ubuntu 22.04 (matches README tested environment)
- PostgreSQL 15 service container
- Automated database setup (creates tinyiot database and user)
- MQTT disabled (no broker available in CI)
- Server started in background with logging
- Runs all 6 test scripts: testCSE.py, testAE.py, testCNT.py, testCIN.py, testCNT_CIN.py, testGRP.py

**Trigger Events**:
- Push to any branch
- Pull request creation
- Manual workflow dispatch

### 3. Created Local Testing Environment

**File**: `docker-compose.yml`

**Features**:
- SQLite test service
- PostgreSQL test service with database container
- Identical environment to GitHub Actions
- Allows local testing before pushing

**Usage**:
```bash
# Test with SQLite
docker-compose run --rm test-sqlite

# Test with PostgreSQL
docker-compose run --rm test-postgresql
```

## Known Issues

### 1. SQLite Build Issue (Resolved)
**Problem**: monitor.c had unconditional PostgreSQL dependencies
**Status**: ✅ Fixed by adding conditional compilation

### 2. PostgreSQL SQL Syntax Error (Unresolved)
**Location**: `source/server/postgresql_implement.c:764-765`

**Problem**:
```c
strcat(sql, ") RETURNING id) INSERT INTO ");
strcat(sql, get_table_name(ty));
strcat(sql, " (id, ");

logger("DB", LOG_LEVEL_DEBUG, "Executing General Table INSERT: %s", sql);
res = PQexec(conn, sql);  // SQL executed here - INCOMPLETE!
```

The SQL is executed before the column list and VALUES clause are added, resulting in:
```sql
INSERT INTO cb (id,   -- Syntax error: incomplete SQL
```

**Impact**: Server fails to start due to database initialization failure

**Error Log**:
```
ERROR [DB]: Failed to insert into general table: ERROR:  syntax error at end of input
LINE 1: ...efaultACP"]',NULL,5,NULL) RETURNING id) INSERT INTO cb (id,
```

**Workaround**: None currently - this is a bug in the project's PostgreSQL implementation

**Note**: The complete SQL generation happens later (line 787+), but execution happens prematurely at line 765.

### 3. MQTT Connection Error (Resolved)
**Problem**: Server shuts down when MQTT broker is unavailable
**Solution**: ✅ Disabled MQTT in CI environment via sed command in workflow

## Modified Files

1. **source/server/monitor.c**
   - Added PostgreSQL conditional compilation
   - Staged for commit

2. **.github/workflows/test.yml**
   - New GitHub Actions workflow
   - Staged for commit

3. **docker-compose.yml**
   - New local testing environment
   - Staged for commit

## Current Status

- ✅ monitor.c fixed for conditional PostgreSQL support
- ✅ GitHub Actions workflow created
- ✅ Docker Compose local testing environment created
- ❌ PostgreSQL server initialization fails (SQL bug)
- ⏸️ Changes staged, pending commit

## Recommendations

### Option 1: Fix PostgreSQL SQL Bug
Modify `postgresql_implement.c` to complete SQL statement before execution.

### Option 2: Use SQLite for CI/CD
Once SQLite build issues are fully resolved, switch to SQLite for simpler testing.

### Option 3: Document as Known Issue
Add the PostgreSQL bug to project documentation and disable PostgreSQL tests until fixed.

## Git Status
```
Changes to be committed:
  modified:   .github/workflows/test.yml
  new file:   docker-compose.yml
  modified:   source/server/monitor.c
```

## Next Steps

1. Decide on approach for PostgreSQL bug
2. Commit changes with appropriate message
3. Push to test branch
4. Monitor GitHub Actions execution
5. Adjust workflow based on results

## Testing Checklist

- [x] monitor.c compiles with PostgreSQL disabled
- [x] GitHub Actions workflow syntax valid
- [x] Docker Compose configuration working
- [ ] PostgreSQL server starts successfully
- [ ] All tests pass in CI environment
- [ ] Documentation updated

## Notes

- config.h should NOT be committed (local environment file)
- PostgreSQL SQL bug exists in original project code
- MQTT requires broker setup (disabled in CI for now)
- Server port 3050 (from config.h default)
