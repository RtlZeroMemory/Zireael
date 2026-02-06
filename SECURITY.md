# Security Policy

## Supported Versions

| Version | Supported |
|---------|-----------|
| 1.x     | Yes       |

## Reporting Vulnerabilities

Preferred path:

- [GitHub Security Advisories](https://github.com/RtlZeroMemory/Zireael/security/advisories/new)

If you cannot use advisories, open an issue requesting a private contact path.

## Response Targets

- acknowledgment: within 48 hours
- initial triage: within 7 days
- remediation timeline: severity and complexity dependent

## In-Scope Issues

- parser overflows / memory corruption
- malformed drawlist/event input leading to undefined behavior
- bounds-check bypasses
- denial-of-service vectors via malformed payloads or unbounded paths

## Out-Of-Scope Issues

- vulnerabilities in third-party terminal emulators
- vulnerabilities in external wrappers/bindings hosted in other repos
- non-production example-code misuse

## Security Design Principles

- all wrapper-provided binary input is untrusted
- bounds checks run before pointer derivation
- parser failures avoid partial side effects
- deterministic limits cap memory/work behavior
- strict platform boundary reduces accidental OS-surface leakage

## Security Testing Expectations

Security-relevant changes should include at least one of:

- unit regression coverage
- fuzz-target coverage extension
- integration test coverage (when backend behavior is involved)
