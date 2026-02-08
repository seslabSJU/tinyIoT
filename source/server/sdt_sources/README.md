# SDT Sources Directory

This directory stores oneM2M Smart Device Template (SDT) XML files.

## Usage

Place SDT XML files in this directory. They will be automatically converted to FCP files during `make`.

### 1. Adding XML Files

- Modify existing XML files with your desired attributes and save them.

### 2. Build

```bash
cd ../  # Navigate to server directory
make sdt  # Convert SDT files only
make      # Full build (SDT conversion + server build)
```

### 3. Verify Output

Generated FCP files are saved in the `../sdt_definitions/` directory.

```bash
ls -l ../sdt_definitions/*.fcp
```

## XML File Naming Convention

SDT XML files must include one of the following domain keywords:

- **Home** → hod (home domain)
- **Common** → cod (common domain)
- **Health** → hed (health domain)
- **City** → cid (city domain)
- **Agriculture** → agd (agriculture domain)
- **Industry** → ind (industry domain)
- **Management** → mad (management domain)
- **Vehicular** → ved (vehicular domain)
- **Railway** → rad (railway domain)
- **PublicSafety** → psd (public safety domain)
- **Metadata** → mdd (metadata domain)

Examples:
- `SDT-TS0023-Devices-Home.xml` → namespace prefix `hod`
- `SDT-TS0023-ModuleClasses-City.xml` → namespace prefix `cid`

## Conversion Tool

tinyIoT uses the official **oneM2M SDTTool**:
- Location: `~/project/sdt-tool/`
- Version: 0.9
- Output format: apjson (FCP)

## References

- SDT Standard: https://www.onem2m.org/
- ts-0023 GitLab: https://git.onem2m.org/XMLSchemas/ts-0023
- SDTTool GitLab: https://git.onem2m.org/tools/sdt-tool
- Shortname mapping: `../tools/shortnames.csv`