# Gundam Museum head icon

Input: user-provided `codex-clipboard-90da3228-51dd-4953-beee-f1cf1724a43c.png`.
Output: `gundam_museum_head.png` (RGBA, 1254 × 1254).
Method: built-in imagegen edit; visual inspection of the extracted artwork and the actual 200 × 200 RGB565 icon. Lower wordmark and checkerboard removed; head, full antenna tips, white outline, red crest/chin and yellow eyes retained.

Deployment: `python3 tools/generate_gundam_museum_icon.py` from the repository root emits `main/assets/images/icon_gundam_museum.png` (transparent preview) and `.c` (80,000 bytes, RGB565 on the existing black launcher tile). The App already references `icon_gundam_museum`; no navigation changes.

## Imagegen prompt

Use case: background-extraction. Edit target: attached Gundam head logo. Extract ONLY the upper robot HEAD illustration, including both complete white V-fin antenna tips and white sticker outline. Remove the entire lower MOBILE SUIT GUNDAM wordmark and all its blue/red outlines. Remove the gray/white checkerboard completely: output actual transparent alpha background, not a checkerboard picture. Preserve the original head geometry, black ink outlines, white helmet, red center crest and chin, yellow eyes exactly; do not redesign, add detail, add text, change expression or invent a new logo. Center the extracted head at large readable size on a square transparent canvas with small even safety margins, all antenna tips intact. Clean antialiased edges, flat original colors, no shadows. This is an existing illustration extraction for an embedded app icon, not a new illustration.
