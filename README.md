# Arcanum Community Edition

> [!IMPORTANT]
> This is a beta, technology preview, or whatever other label you have in mind for a pre-release software.

> **MODIFICATION NOTICE:** This project is a modified, derivative work of the open-source [Arcanum Community Edition](https://github.com/alexbatalov/arcanum-ce) by Alexander Batalov. It is distributed free of charge for non-commercial purposes in accordance with the Sustainable Use License. All original copyright notices and licenses have been preserved. 

# Arcanum New Balance — ARPG Obscura

**Arcanum New Balance — ARPG Obscura** is a comprehensive gameplay overhaul for *Arcanum: Of Steamworks and Magick Obscura*, built on top of the open-source Arcanum Community Edition reimplementation by Alexander Batalov. It transforms Arcanum's classic RPG systems into a deep action-RPG experience without changing the original story, world, or writing.

## Features

### ARPG Itemization
Every item in the world now rolls with a rarity tier — **Common, Uncommon, Rare, Epic, Unique, Set, and Cursed** — each granting random affixes that modify stats, resistances, damage, and skills. 
* Six item sets are hidden across the world; equipping multiple pieces of a set unlocks escalating bonuses and on-hit procs. 
* Bosses have guaranteed unique drops *(not yet implemented)*. 
* A new floating tooltip shows all stats, affixes, and set progress at a glance. 
* Unique items are build-enabling and very strong — the chance for them to appear in shops/NPC inventories is 0.1%.

### ARPG Crafting
Monsters with rarity can drop magical orbs on the ground on top of their body. These orbs are used by moving them in the inventory onto equippable (non-quest) items. There are 8+ orbs in total, including:
* **Orb of Awakening:** Recreates a white item with rarity.
* **Identification Scroll:** Reveals the rarity on an item.
* **Cursed Orb:** Modifies an item unpredictably.
* **Orb of Cleansing:** Removes rarity from an item.
* *And more… similar to Path of Exile (Divine Orbs, etc.)*

You can craft items up to EPIC quality. Sets and Uniques can only be dropped, not crafted.

### Overhauled Magic
All 16 schools of magic now have a distinct combat identity. Every school's signature spell deals damage through a unique mechanic:
* **Fire:** Ignites for burn-over-time.
* **Force:** Chains lightning to nearby enemies.
* **Water:** Slows and freezes.
* **Earth:** Poisons.
* **Mental:** Confuses.
* **Phantasm:** Terrifies.
* **Temporal:** Acid-burns and stuns.
* **Nature:** Transform forms (Wolf, Lizard, Bear) are fully redesigned as a melee-damage school — each form trades mobility or survivability for powerful on-hit effects.

*(Inspiration: Arcanum Equalibrium / Terror's Arcanum by TheDragon13 - heavily changed. Not all spells are changed, and some strong spells are untested.)*

**Blood Magic:** The Necromancy school was always busted. So I changed the mana for life. It's actually funny and still strong. Enjoy.

### Tech Rebalance
Firearms are heavily rebalanced with unique distinct combat identities:
* **Knockback:** 15% knock prone, 5% knockdown (sniper / elephant gun / tech hammer).
* **Shotgun:** Multi-target spread hit.
* **Flamethrower:** AoE splash damage and burn ticks.
* **Pyro axe / gun:** Bonus fire damage.
* *And more…*

*(Inspiration: Shark - community member on Arcanum Discord - with permission. Thank you!)*

### Monster Rarity
Enemies roll as Magic, Rare, or Unique with randomized bonus types: Vampiric, Thorned, Swift, Berserker, Shielded, and more. Unique monsters have procedurally generated titles. Difficulty and loot quality scale with monster rarity. Rare monsters are especially important for orb drops used in crafting.

### New Playable Races
Dark Elf, Half-Ogre, Orc, and Lizard Man are restored from cut content and fully playable, with working body types, portraits, and dialog reactions. Each has unique racial stats matching their lore.
*(Implemented from UAP by Drog Black Tooth + additional Orc art files from F-man - with permission. Thank you!)*

### New Weapon Classes
* **Unarmed weapons** (gauntlets, claws) with scaling damage and on-hit effects.
* **Throwing weapons** with damage-over-time properties by type (bleed, poison, fire).
* **Staves** that channel spells on equip, granting lower-tier college spells as active abilities.
* **Thorns items** (chest, boots, helmet, shield).

### Quality of Life & Engine Updates
* An in-game **F9 overlay** documents all mod systems for new players. 
* Engine bugs from the original game are fixed throughout. Many UAP fixes re-applied to CE.
* **New Stats:** Life on Hit, Poison on Hit, Fire on Hit, Acid on Hit, Bleed on Hit, Thorns, and more — visible in the character sheet.
* **Tooltip system:** Custom tooltips can display much more text. *(Note: There is a bug that will sometimes leave graphical fragments on item hover until the inventory is closed — not game-breaking, will be fixed later.)*

## Map System & Endgame Rift
A work-in-progress map system inspired by Path of Exile. Vormatown is implemented into the base game *(F-Man, with permission)*. Level cap is 99. After the slideshow, the game ports you back to Shrouded Hills and gives you your first map. Maps work like orbs — move onto any equippable item to teleport into a map zone with strong enemies.

### Entering the Rift
* **Obtain Map:** Hunt high-level monsters (Level 30+) in the main world. Magic, Rare, and Unique enemies have increased drop rates for the Map of the Void.
* **Activate:** Right-click the Map of the Void to consume it and open a Rift.

### Inside the Rift
* **Distinct Themes:** Every Rift rolls one of 5 types: Physical, Fire, Poison, Magic, or Void. The map layout displays thematic scenery and applies matching combat modifiers.
* **Scaled Difficulty:** Fight customized pools of monsters (demons, golems, void lizards, automatons). Spawn counts and monster tiers scale up with the active Rift Tier.

### Rewards & Exit
* **Loot Chest:** Clearing all monsters spawns a Reward Chest containing scaling gold, crafting orbs, high-rarity equipment, a new Map of the Void (pre-upgraded to the next Tier), and a Void Portal Stone.
* **Return Home:** Right-click the Void Portal Stone (consumed) or the Map of the Void to teleport back to your exact entry point in the main world.

## Screenshots
[*(Mod Page)*](https://brenek.art/releases/arcanum-new-balance.html)

## Installation
[*(Mod Page)*](https://brenek.art/releases/arcanum-new-balance.html) Simply download the latest release and extract all files into main game directory. Raplace all when prompted.

## Requirements
Own the original game.

## Shoutouts
Big thanks to everyone keeping Arcanum alive over the years:
* **Alexander Batalov** — Arcanum Community Edition
* **sevdestruct** — UI fixes for Arcanum CE
* **Drog Black Tooth** — UAP
* **F-Man** — Orcs art, Vormatown
* **Shark** — Tech rebalance inspiration
* **TheDragon13** — Arcanum Equalibrium / Terror's Arcanum
* **CDude, harleyfolgado, Barbarbrick, Jen,** and everyone on the community Discord. 

— *Viktor*

## Status

The game is ready, but I haven't tested everything - just the speedrun - and that was about three months ago, so things might have changed since then.

Regarding the source code, about half of the modules (not half of the code) are in good shape. All APIs have meaningful names, along with brief documentation, annotations, and explanations (see `skill.c` and `quest.c` as examples). The other half may have cryptic names, little to no symbols , and no documentation at all (`anim.c` is extremely large and hard to understand).

## Installation (if you want to build yourself, but it will not work, without my modified .art files .protos and other stuff, that are not distributed via github yet)

You must own the game to play. Purchase your copy on [GOG](https://www.gog.com/game/arcanum_of_steamworks_and_magick_obscura) or [Steam](https://store.steampowered.com/app/500810).

<details>
    <summary>Minimum installation</summary>

    ```
    .
    ├── arcanum1.dat
    ├── arcanum2.dat
    ├── arcanum3.dat
    ├── arcanum4.dat
    ├── modules
    │   ├── Arcanum
    │   │   ├── movies
    │   │   │   ├── 00069.bik
    │   │   │   ├── 01138.bik
    │   │   │   ├── 02112.bik
    │   │   │   ├── 50000.bik
    │   │   │   ├── 51169.bik
    │   │   │   ├── 91568.bik
    │   │   │   ├── A0021.bik
    │   │   │   ├── G0021.bik
    │   │   │   └── movies.mes
    │   │   └── sound
    │   │       └── music
    │   │           ├── Arcanum.mp3
    │   │           ├── Caladon.mp3
    │   │           ├── Caladon_Catacombs.mp3
    │   │           ├── Cities.mp3
    │   │           ├── Combat 1.mp3
    │   │           ├── Combat 2.mp3
    │   │           ├── Combat 3.mp3
    │   │           ├── Combat 4.mp3
    │   │           ├── Combat 5.mp3
    │   │           ├── Combat 6.mp3
    │   │           ├── CombatMusic.mp3
    │   │           ├── Dungeons.mp3
    │   │           ├── DwarvenMusic.mp3
    │   │           ├── Interlude.mp3
    │   │           ├── Isle_of_Despair.mp3
    │   │           ├── Kerghan.mp3
    │   │           ├── Mines.mp3
    │   │           ├── Qintara.mp3
    │   │           ├── Tarant.mp3
    │   │           ├── Tarant_Sewers.mp3
    │   │           ├── Towns.mp3
    │   │           ├── Tulla.mp3
    │   │           ├── Vendegoth.mp3
    │   │           ├── Villages.mp3
    │   │           ├── Void.mp3
    │   │           └── Wilderness.mp3
    │   ├── Arcanum.PATCH0
    │   ├── Arcanum.dat
    │   └── Vormantown.dat
    └── tig.dat
    ```
</details>

### Windows

Download and copy `arcanum-ce.exe` to your `Arcanum` folder. It serves as a drop-in replacement for `arcanum.exe`.

### Linux

- Use the Windows installation as a base - it contains the data assets needed to play. Copy the `Arcanum` folder somewhere, for example `/home/john/Desktop/Arcanum`.

- Alternatively, you can extract the required files from the GOG installer:

```console
$ sudo apt install innoextract
$ innoextract ~/Downloads/setup_arcanum.exe -I app
$ mv app Arcanum
```

- Download and copy `arcanum-ce` to this folder.

- Run `./arcanum-ce`.

### macOS

> [!NOTE]
> macOS 10.13 (High Sierra) or higher is required. Runs natively on Intel-based Macs and Apple Silicon.

- Use the Windows installation as a base - it contains the data assets needed to play. Copy the `Arcanum` folder somewhere, for example `/Applications/Arcanum`.

- Alternatively, if you have Homebrew installed, you can extract the required files from the GOG installer:

```console
$ brew install innoextract
$ innoextract ~/Downloads/setup_arcanum.exe -I app
$ mv app /Applications/Arcanum
```

- Download and copy `Arcanum Community Edition.app` to this folder.

- Run `Arcanum Community Edition.app`.

### Android & iOS

These ports are not currently intended for players. Touch controls are not yet implemented, and window management is subpar. No further instructions will be provided until these issues are resolved (but you can easily figure it out, it's not rocket science).

## Building from source

Check [`ci-build.yml`](.github/workflows/ci-build.yml) for details on how the project is compiled.

## Configuration

Several configuration options are available as command-line switches (admittedly not very user-friendly):

- `-4637`: Enable cheat level 3

- `-window`: Run in windowed mode (default is fullscreen)

- `-geometry=1280x720`: Set window size (default is 800x600)

When `HighRes/config.ini` from the Unofficial Arcanum Patch is present in the game directory, Community Edition also imports `Width`, `Height`, `Windowed`, `ShowFPS`, `ScrollFPS`, `ScrollDist`, `Logos`, and `Intro` at startup.

## Contributing

Play the game and file bugs if any (there are likely many). Attach a save game for investigation. Suggestions for quality of life improvements are also welcome. The major objective for 25H2 is to clarify remaining functions.

## License

The source code is this repository is available under the [Sustainable Use License](LICENSE.md).
