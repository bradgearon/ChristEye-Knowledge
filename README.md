> **WDS research property** — Copyright © 2026 We Develop Software LLC. Public research and experimental materials are governed by the [WDS Public Research Reference License](LICENSE.md). Patent contents are intentionally not included. See [publication policy](PUBLICATION_POLICY.md).

# ChristEye Anaiero — v6 Engineering & Documentation Release

**November 15, 2026 · Phase P02B**

> A modular developmental-computing research architecture with tested *software* interfaces and separately gated living-computer hypotheses. No living neural hardware has been constructed or validated by this release.

## Project navigation

- [GitHub-readable documentation home](website/content/index.md)
- [Master P02B architecture](phases/P02B_Supervisory_Electronics_and_Scientific_Operations/docs/00_P02B_Master_Engineering_Architecture.md)
- [Revised roadmap](website/content/phases.md)
- [Board specifications](website/content/hardware.md)
- [Scientific director and cell foundry](website/content/science.md)
- [Paired module designs and companions](website/content/modules/index.md)
- [Current code and tests](phases/P02B_Supervisory_Electronics_and_Scientific_Operations/README.md)
- [Decision and conversation history](website/content/history.md)
- [Archival Word and source-log inventory](website/content/archives.md)

## Local demo

```bash
cd phases/P02B_Supervisory_Electronics_and_Scientific_Operations
python -m unittest discover -s tests -v
python run_demo.py
```

## GitHub Pages

The repository contains a pre-built static site in `website/site/`. Enable **Settings → Pages → Source: GitHub Actions** and push to your chosen repository; `.github/workflows/pages.yml` will deploy the site. The same source exists as plain GitHub-flavored Markdown in `website/content/`. To rebuild the site locally, run `python website/build_site.py` (requires the `mistune` Python package).

## Privacy and historical records

The **full private research ZIP** also includes original private session logs and all distinct historical Word files. Never publish raw logs without reviewing personal data and permissions. `private_archive/` is intentionally gitignored and excluded from Pages. Full ChatGPT verbatim export is not available through this workflow; curated user excerpts are marked as such.

## Boundary

The read-only hardware supervisor is a **digital twin**; the Nucleo/ESP32/AD5940 choices are candidate dev boards. Biological feeding, peptide-controlled behavior, organoid growth and physical Crystal transfer are research-only and require independent scientific and laboratory oversight.