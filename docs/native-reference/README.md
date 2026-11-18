# Anaiero Native C++ Reference

The reference at [the GitHub Pages URL](https://bradgearon.github.io/ChristEye-Knowledge/native-reference/) is the full browsable site from the supplied **anaiero-native-reference** package, not the former header crawler.

## Contents

- `index.html` — three-panel API navigation, filters, tree browsing and offline export.
- `styles.css` and `app.js` — the supplied frontend.
- `data-parts/00.txt` through `23.txt` — exact contiguous segments of the supplied `data.js` (5,030 indexed declarations from the pinned source snapshot).
- `offline-parts/00.txt` through `23.txt` — exact contiguous segments of the supplied self-contained HTML edition, downloadable from the site.
- Original source repository: `bradgearon/ChristEye-Knowledge`. The browser preserves the source commit and branch identifiers recorded by the supplied package.

GitHub's file API rejected uploading 7+ MB assets as single blobs. The browser reconstructs each source asset in order from 24 static parts, requiring no backend or build pipeline. The downloadable standalone reference is reconstructed to an ordinary `.html` file in the browser.

Source descriptions, declared functions, class ownership and overloads come from the supplied package. The index is a **snapshot**, not an assertion that it was regenerated from today's branch head.
