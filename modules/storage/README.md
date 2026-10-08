# Module `storage`

JSON files (nlohmann/json):

* projects (`*.rcproj`) and workshop profiles (`*.rcprofile`): the design, not derived geometry or drilling; missing fields fall back to defaults (stage 5);
* the stock of sheets and remnants (`stock.rcstock`) and the cutting settings (`settings.json`) in the user folder, `userDataFolder()` (stage 7).

Every file is written to `<name>.tmp` and then renamed over the old one, so a crash never leaves a half-written file.

**Stages:** 5, 7
