# Anaiero v3 — architecture diagram

```
USER / APPROVED HOST DATA
       |
 A19 SOURCE ROUTER (permissions + hashes)
       |
 A01 EPISODE JOURNAL --> A02/A20 DURABLE STORE --> A16 ROOTS
       |                                     |
 A07 CURRICULUM ---------------------- A06 RETENTION
       |                                     |
       +------ A12 LOTUS GOVERNOR <-----------+
                        |
                A22 LIMITS / VETO
                        |
                   A11 WINDBUS
                        |
      DIGITAL STARCHIPS / A09 RESEARCH BRIDGE
                        |
                   A24 OBSERVERS
                        |
                     EVALUATOR
                        |
      A10 CRYSTAL COMPILER -> A13 CLONE -> A14 CONSUME/REPLAY
```

Health, care and growth networks A03/A04/A05/A08/A17 feed measured **advisory state** to Lotus and independent Limit L; they do not own biological care actions. See master DOCX and per-module documents for full contracts.