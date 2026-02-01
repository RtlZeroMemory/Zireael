# Security Policy

## Supported Versions

| Version | Supported |
|---------|-----------|
| 1.x     | Yes       |

## Reporting Vulnerabilities

**Preferred:** Use [GitHub Security Advisories](https://github.com/RtlZeroMemory/Zireael/security/advisories/new)

**Alternative:** Open a GitHub issue requesting private contact if you cannot use Security Advisories.

## Response Timeline

- Acknowledgment within 48 hours
- Initial assessment within 7 days
- Fix timeline depends on severity and complexity

## Scope

Security issues in Zireael include:

- Buffer overflows in drawlist/event parsing
- Memory corruption in core engine
- Input validation bypasses
- Denial of service via malformed input

Out of scope:

- Issues in example code (not production)
- Terminal emulator vulnerabilities
- Wrapper/binding issues (separate projects)

## Security Design

Zireael treats all wrapper-provided input as untrusted:

- Drawlist bytes are validated before execution
- Event buffers use bounded writes
- No partial effects on validation failure
- Fixed resource limits prevent exhaustion
