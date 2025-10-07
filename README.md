# 📜 SKSE "OBody NG Preset Distribution Assistant NG"

It is a simple but powerful SKSE DLL that processes OBody preset distribution configuration files from INI, paired with SPID to manage presets to the Master OBody_presetDistributionConfig.json automatically without direct intervention. It was created with the objective of automating the application of preset rules for the OBody mod in Skyrim Special Edition, the purpose of this is to facilitate modders in delivering an OBody for their characters, and also adding a default preset which users can easily change from their game.

---

# What does it do?

**For more details, including a rule generator tool, visit the [OBody NG Preset Distribution Assistant NG Wiki](https://john95ac.github.io/website-documents-John95AC/OBody_NG_Preset_Distribution_Assistant_NG/index.html).**

Upon data loaded, scans Data for OBodyNG_PDA_*.ini files, parses rules (Key = Keyselec|PresetA,PresetB,...|Mode), applies to OBody_presetDistributionConfig.json preserving order.

Modes:

```
- " x " : Unlimited – adds presets to JSON every execution (does not change INI); if already exist, no changes.
- " 1 " : Once – adds presets, updates INI to |0 (applies only once, then deactivates).
- " 0 " : No apply – skips rule and logs "Skipped"; INI deactivated, no JSON changes.
- " - " : Removes specific preset – searches with "!" (e.g., "!CCPoundcakeNaked2" removes "CCPoundcakeNaked2"), updates INI to |0.
- " x- " : Removes specific preset every execution – (does not change INI); if already removed, no changes.
- " * " : Removes complete entry from key (e.g., npc or race) – eliminates plugin (e.g., "YurianaWench.esp" and presets), updates INI to |0.
- " x* " : Removes complete entry from key every execution – (does not change INI); if already removed, no changes.
- Any number >=2 or invalid element: Treated as "1" – applies once, updates INI to |0.
```

If JSON cannot be read (e.g., due to execution path discrepancy), process stops for stability to avoid CTD or problems.

Logs actions and summary in OBody_NG_Preset_Distribution_Assistant_NG.log.

## INI Rules Examples

```
;OBody_NG_Preset_Distribution_Assistant_NG

;Example of code designs: it's very similar to SPID but shorter and simpler.

; npcFormID = xx0001|Preset,...|x , 1 , 0 , - , *             FormID
; npc = EditorID|Preset,...|x , 1 , 0 , - , *                    EditorID name like 000Rabbit_NPC or Serana
; factionFemale = Faction|Preset,...|x , 1 , 0 , - , *           Faction name like "ImperialFaction" or "KhajiitFaction"
; factionMale = Faction|Preset,...|x , 1 , 0 , - , *
; npcPluginFemale = Plugin.esp|Preset,...|x , 1 , 0 , - , *       The name of the esp, who has a defined body
; npcPluginMale = Plugin.esp|Preset,...|x , 1 , 0 , - , *
; raceFemale = Race|Preset,...|x , 1 , 0 , - , *                Work with "NordRace", "OrcRace", "ArgonianRace", "HighElfRace", "WoodElfRace", "DarkElfRace", "BretonRace", "ImperialRace", "KhajiitRace", "RedguardRace", "ElderRace"  or  Works with custom races too
; raceMale = Race|Preset,...|x , 1 , 0 , - , *

; More information about the JSON that these adjustments are applied to in the end is here.
; https://www.nexusmods.com/skyrimspecialedition/articles/4756
```

## Updates and Revisions

**Version 1.0.0:**

This is the first version of the DLL, introducing the core functionality for processing INI rules to automate OBody preset distribution. It establishes the foundation for safe JSON updates while preserving order and existing data.

Key INI rules explained:

- " x " **(Unlimited Add)**: Adds the specified presets to the JSON every time the plugin loads, without modifying the INI file. If the presets already exist for the plugin, no duplicates are added, ensuring idempotent behavior for repeated executions in mod load orders or updates.
- " 1 " **(Once Add)**: Adds the presets to the JSON only one time, then automatically updates the INI rule to "|0" to deactivate it, preventing re-application on subsequent loads. Ideal for one-off distributions in mod installations.
- " 0 " **(Disabled)**: Completely skips the rule, logging it as "Skipped" without any JSON changes. This allows modders to disable rules temporarily or permanently without deleting lines, useful for debugging or user customization.

**Version 1.2.0:**

An error was detected in some versions of Windows where the code only read files using ANSI code pages, leading to failures in handling paths with non-ASCII characters (e.g., accented letters or international symbols in mod folders). To address this, a Unicode method using UTF-8 was included as a primary encoding option, with fallbacks to system ANSI and ASCII-safe conversion. This update improves compatibility on legacy Windows setups (e.g., Windows 7/8 in non-English locales) and prevents CTDs from encoding mismatches, laying the groundwork for the full Unicode enhancements in subsequent versions.
Advanced removal options introduced in this version for more granular control:

- " - " **(One-Time Preset Remove)**: Removes specific presets from the JSON (using "!" prefix in preset names, e.g., "!BadPreset" to remove "BadPreset"), then updates the INI rule to "|0" to deactivate it. Applies only once, ideal for cleaning up specific presets in mod updates without repeated execution.
- " x- " **(Unlimited Preset Remove)**: Removes specific presets every time the plugin loads, without modifying the INI file. If the presets are already removed, no changes are made, ensuring idempotent behavior for ongoing maintenance in dynamic mod setups.
- " * " **(One-Time Plugin Remove)**: Removes the entire plugin entry (all presets for that plugin) from the key in the JSON, then updates the INI rule to "|0". Applies only once, useful for completely disabling a plugin's presets in a single pass.
- " x* " **(Unlimited Plugin Remove)**: Removes the entire plugin entry every time the plugin loads, without modifying the INI file. If the plugin is already removed, no changes occur, perfect for persistent removal across load orders or updates.

**Version 1.5.3:**

  the plugin now includes enhanced Unicode support via UTF-8 and ANSI fallbacks in the `SafeWideStringToString` function, ensuring robust handling of file paths and strings to prevent crashes and encoding errors across diverse Windows systems. Not all PCs have consistent ANSI (code page) support, which can result in CTDs due to logic errors when dealing with non-ASCII characters in mod paths or international locales. This implementation prioritizes UTF-8 for modern compatibility, falls back to the system's ANSI code page, and uses an ASCII-safe conversion as a final resort to maintain stability in varied Skyrim modding environments.

Special thanks to the beta testers who dedicated their valuable time to thoroughly test the DLLs, identify issues, and collaborate on solutions. Your contributions have been essential in refining this plugin for reliability.

**Version 1.6.0:**

This version incorporates additional revisions to the DLL processing logic, enhancing overall stability, error handling, and performance during INI rule parsing and JSON modifications.

A new one-time backup system has been added to safeguard the original OBody_presetDistributionConfig.json by creating an exact copy before processing any INI rules. This prevents data loss during initial mod setups or updates.

The system is configured via 'OBody_NG_Preset_Distribution_Assistant_NG.ini' in the DLL's directory (Data/SKSE/Plugins/). If the file doesn't exist, it is automatically created with default settings.

INI Configuration Example:

```
[Original backup]
Backup = 1  ; Set to 1 to enable (default); 0 to disable. After first use, automatically set to 0.
```

How it Functions (One-Time Operation):
- On plugin load, if Backup = 1 and the JSON exists: Creates the backup folder (Data/SKSE/Plugins/Backup/) if needed, copies the JSON to Backup/OBody_presetDistributionConfig.json (exact copy, overwrites if exists), verifies file sizes match, then updates the INI to Backup = 0.
- If Backup = 0: Skips backup and logs that it was already performed (or disabled).
- The backup allows manual recovery by copying it back to OBody_presetDistributionConfig.json.
- All actions (creation, verification, updates) are logged in OBody_NG_Preset_Distribution_Assistant_NG.log for transparency.
- This ensures a safe, automated initial backup without repeated operations, ideal for mod installations in complex setups.

**Version 1.7.0:**

This version introduces critical robustness and security improvements to handle large JSON files and prevent data corruption, thanks to Cryshy's detection of a bug with JSON files over 7000 lines that caused crashes or incomplete reads.

Key improvements applied:

- **Triple JSON Integrity Validation**: Before any read or write, the plugin performs an exhaustive check: (1) Basic structure (minimum 10 bytes size, starts and ends with `{}`), (2) Balance of braces `{}` and brackets `[]` (counting correctly inside strings, ignoring escapes like `\"`), (3) Presence of at least 6 of the 8 expected keys (npcFormID, npc, factionFemale, factionMale, npcPluginFemale, npcPluginMale, raceFemale, raceMale). If any validation fails, automatic restoration from backup is triggered to avoid processing corrupted data.

- **Advanced Support for Backup with "true"**: In the configuration INI file (OBody_NG_Preset_Distribution_Assistant_NG.ini), `Backup=true` is now supported for "always backup" mode. In this mode, the backup is performed on every plugin execution (copying the original JSON to /Backup/ each time), and the INI is not automatically modified (remains true). This is ideal for development or testing environments where a constant snapshot is needed. The previous mode (Backup=1) remains one-time (changes to 0 after first use), and Backup=0 fully disables.

Updated INI configuration example with "true" support:

```
[Original backup]
Backup = true  ; Always backup mode: Performs literal backup every execution, without auto-disabling.
; Backup = 1    ; One-time backup (default): Enables backup once, then sets to 0.
; Backup = 0    ; Disabled: Skips backup entirely.
```

- **Support for Larger JSONs**: The file size limit has been increased to 50MB (previously 1MB), with binary reading (`std::ios::binary`) to handle special encodings without alterations. Plugin and preset parsing uses optimized loops with `reserve()` on vectors (e.g., 200 for results, 50 for presets per plugin) for memory efficiency. The bug reported by Cryshy in JSONs over 7000 lines is resolved, where the previous manual parsing failed on iterations or complex escapes; now it correctly handles escapes (e.g., backslashes in preset names) and raised iteration limits to 100,000 with safeguards against infinite loops.

- **Automatic Restoration and Forensic Analysis**: If corruption is detected in the original JSON (during reading or post-writing), the plugin automatically restores from the backup (only if the backup passes validation), moves the corrupted file to the analysis folder, and retries the process. This prevents CTDs and data loss in setups with conflicting mods.

- **Preservation of Original Format**: The update only modifies the 8 valid keys, preserving indentation, whitespace, and unrelated sections (e.g., comments or data from other mods). It uses precise search and replace instead of full rewriting, maintaining the order and structure of the existing JSON.

These improvements make the plugin much more resistant to common Skyrim modding failures, such as interruptions during writes or JSONs generated by external tools with irregular formats. Logging now includes details on validations, file sizes, and restoration actions for easy debugging.

**Version 1.7.12:**

This version introduces a key optimization in the processing flow to significantly improve DLL performance, especially in setups with stable mods and large JSONs.

New step in the flowchart:
- **INI Changes Verification**: Before applying any modifications to the master JSON, the plugin compares the current file state with the processed data from INI rules. If it detects new mods included or changes in configurations (e.g., new entries in OBodyNG_PDA_*.ini), it applies the corresponding updates. If there are no changes (i.e., the rules are already implemented and no new mods have been added), it simply verifies the JSON integrity without making unnecessary modifications.

Performance benefits:
- Drastically reduces JSON write operations for repeated game loads, avoiding redundant rewrites that consume CPU and load time.
- Improves efficiency in environments with many mods, where the JSON can be large (up to 50MB), preventing potential CTDs from unnecessary processing.
- Logging now includes details on change verification, allowing users to monitor when operations are skipped for optimization.

This optimization makes the DLL more efficient and stable, aligning with the goal of automation without manual intervention.

## Acknowledgements

### Beta Testers

<table>
<tr>
<td><img src="Beta Testers/OpheliaMoonlight.png" width="100" height="100" alt="OpheliaMoonlight"></td>
<td><img src="Beta Testers/IAleX.png" width="100" height="100" alt="IAleX"></td>
<td><img src="Beta Testers/Thalzamar.png" width="100" height="100" alt="Thalzamar"></td>
<td><img src="Beta Testers/Cryshy.png" width="100" height="100" alt="Cryshy"></td>
<td><img src="Beta Testers/Lucas.png" width="100" height="100" alt="Lucas"></td>
<td><img src="Beta Testers/storm12.png" width="100" height="100" alt="storm12"></td>

<td><img src="Beta Testers/Edsley.png" width="100" height="100" alt="Edsley"></td>
</tr>
</table>

I also want to extend my deepest gratitude to the beta testers who generously dedicated their valuable time to help me program, test, and refine the mods. Without their voluntary contributions and collaborative spirit, achieving a stable public version would not have been possible. I truly appreciate how they not only assisted me but also supported the broader modding community selflessly. I love you all, guys **Cryshy**, **IAleX**, **Lucas**, **OpheliaMoonlight**, **storm12**, **Thalzamar**, and **Edsley** your efforts have been invaluable, and I'm incredibly thankful for your dedication.

Special thanks to **Edsley** for providing helpful instructions and support for web-related aspects during development and testing.

Special thanks to **Cryshy** for identifying a critical issue with handling large JSON files (over 7000 lines), which led to the implementation of enhanced parsing, memory limits, and validation in Version 1.7.0. Your sharp eye for bugs made the plugin far more robust!

A special shoutout to **OpheliaMoonlight** for testing the program in the beta phase. In her case, particularly, the programming languages did not support Unicode initially, so after many hours of trial and error, we managed to make it work on multiple platforms. Thank you so much!

Special thanks to the SKSE community and CommonLibSSE developers for the foundation. This plugin is based on SKSE templates and my custom parsing logic for INI and JSON. Thanks for the tools that make modding possible.

# CommonLibSSE NG

Because this uses [CommonLibSSE NG](https://github.com/CharmedBaryon/CommonLibSSE-NG), it supports Skyrim SE, AE, GOG, and VR.

[CommonLibSSE NG](https://github.com/CharmedBaryon/CommonLibSSE-NG) is a fork of the popular [powerof3 fork](https://github.com/powerof3/CommonLibSSE) of the _original_ `CommonLibSSE` library created by [Ryan McKenzie](https://github.com/Ryan-rsm-McKenzie) in [2018](https://github.com/Ryan-rsm-McKenzie/CommonLibSSE/commit/224773c424bdb8e36c761810cdff0fcfefda5f4a).

# Requirements

- [SKSE - Skyrim Script Extender](https://skse.silverlock.org/)
- [OBody Next Generation](https://www.nexusmods.com/skyrimspecialedition/mods/77016)
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444)
