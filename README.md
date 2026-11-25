# Haiven for Lyra

Haiven's plugins for Epic's [Lyra Starter Game](https://dev.epicgames.com/documentation/unreal-engine/lyra-sample-game-in-unreal-engine)
on Unreal Engine 5.8: sign-in with Haiven, chat, the message of the day,
inventory from Haiven entitlements, and reward claims.

## Set up the demo

You need Unreal Engine **5.8.2** and your own **Lyra Starter Game** project.
This repository provides the Haiven plugin source and the files to add to
that project.

1. In Epic Games Launcher, install Unreal Engine 5.8.2. Add
   [Lyra Starter Game](https://www.fab.com/listings/93faede1-4434-47c0-85f1-bf27c0820ad0)
   to your Fab library, accept Epic's terms when prompted, then choose
   **Create Project** from **Unreal Engine > Library > Fab Library**.
2. Clone this repository. In the project folder containing `Lyra.uproject`,
   copy these folders from the clone, keeping their paths. Merge the folders
   into the project's existing `Plugins` and `Content` directories.

   | Copy from the clone | Place in the Lyra project |
   |---|---|
   | `Plugins/Haiven` | `Plugins/Haiven` |
   | `Plugins/HaivenLyraDemo` | `Plugins/HaivenLyraDemo` |
   | `Plugins/HaivenLyraInventory` | `Plugins/HaivenLyraInventory` |
   | `Content/HaivenTestFixtures` | `Content/HaivenTestFixtures` |

3. In `Lyra.uproject`, add these entries to the existing `Plugins` array.
   If the project already lists a plugin, set its `Enabled` field to `true`.

   ```json
   { "Name": "Haiven", "Enabled": true },
   { "Name": "HaivenLyraDemo", "Enabled": true },
   { "Name": "HaivenLyraInventory", "Enabled": true }
   ```

   For a clean Fab project outside another Git checkout, Git can do this
   step for you. Run from the project folder and replace the path with the
   path to the patch in your clone.

   ```sh
   git apply --verbose "/path/to/haiven-sdk-unreal-lyra/patches/0001-feat-lyra-enable-the-Haiven-plugins.patch"
   ```

4. Open `Lyra.uproject` in Unreal Editor 5.8.2 and build the modules when
   prompted. Open `L_LyraFrontEnd`, click **Play** in the editor, then select
   **PLAY** in the Haiven panel to start an offline match.

## Haiven releases and versioning

Releases are published on this repository's [GitHub Releases page](https://github.com/haiven-dev/haiven-sdk-unreal-lyra/releases). The Haiven plugins in this repository are published from Haiven's SDK source, and Haiven's release pipeline builds and publishes every release from that source after a maintainer review. Nothing in this repository cuts a release: there is no release workflow, release pull request, or tag to push here.

Each release carries `haiven-lyra-demo-plugin-<version>.zip` (the HaivenLyraDemo plugin), `haiven-lyra-inventory-plugin-<version>.zip` (the HaivenLyraInventory plugin), and `SHA256SUMS`, the SHA-256 digest of every asset in `sha256sum` format. Each zip has one top-level folder named after its plugin, for `<Project>/Plugins/`. The Lyra project itself is Epic sample code, which releases do not attach. The release notes list each asset with its digest and name the source commit it was built from.

A release is tagged `v<version>`. Haiven's SDK and demo repositories share one version line, which follows [Semantic Versioning](https://semver.org/): the repositories in one release carry the same version, so this demo's release has the same major.minor as the [haiven-sdk-unreal](https://github.com/haiven-dev/haiven-sdk-unreal) release it ships with. A release can cover only some of the repositories, so a fix or feature that only affects this demo can ship without a haiven-sdk-unreal release, and this repository's version numbers can skip. A version with a pre-release suffix (`v1.2.3-rc.1`) is published as a GitHub pre-release and is never marked Latest.

The release tag and the asset names carry the version. `VersionName` in the HaivenLyraDemo and HaivenLyraInventory descriptors comes from Haiven's source and is not the demo's release number. `Plugins/Haiven/Haiven.uplugin` is a copy of the haiven-sdk-unreal plugin, so its `VersionName` is that plugin's.

Source updates arrive as pull requests into `main`. Contributions are welcome as pull requests here too; see [CONTRIBUTING.md](CONTRIBUTING.md). Haiven's maintainers carry each accepted change to the Haiven plugins into Haiven's source, so later updates keep it.

## Licensing and contributing

Lyra is covered by the Unreal Engine license that came with your copy of it.
See the [Lyra sample documentation](https://dev.epicgames.com/documentation/unreal-engine/lyra-sample-game-in-unreal-engine).
Lyra itself is not in this repository.

Haiven's files here are licensed under the Apache License, Version 2.0 (SPDX
`Apache-2.0`): the plugins under `Plugins/`, the fixtures in
`Content/HaivenTestFixtures`, and Haiven's changes in `patches/`. The terms are
in [LICENSE](LICENSE), and each plugin directory carries the same text in its
`LICENSE.md`. The license does not cover the Lyra files the patches apply to.

Review changes to the Haiven plugins as GitHub pull requests. This repository
runs no checks of its own, and nothing here cooks Lyra; that needs Lyra's Fab
content and an Unreal install.

## Playing

In Unreal Editor, open `L_LyraFrontEnd` and start Play. Set **Editor Startup
Map** to `L_LyraFrontEnd` under **Project Settings → Maps & Modes** to open
it by default. A standalone game starts on the front end. Select **PLAY** on
the left to host a local offline match, the way Boss Room's Host starts play.
**PLAY** requires the Fab content.

### Chat and panel controls

- **Enter** during play opens **CHAT**, focuses the message field, and
  releases the mouse so you can click the Haiven controls. If the pointer
  is already free, use the **CHAT**, **REWARDS**, or **MOTD** buttons.
- **Enter** in the message field sends a non-empty message. Once the demo
  queues it, keyboard and mouse control return to the game.
- **Escape** while the message field has focus, or **Enter** on an empty
  field, returns control to the game without sending a message.

### Message of the day and rewards

On the front end or in a match, use the Haiven buttons at the upper right.
If the game has captured the mouse, press **Enter** first to free it.

- Select **MOTD** to read the message of the day. **MOTD • NEW** marks an
  unread message. You can read it before signing in. The button appears
  when the demo has a **Base URL** and **Integration ID**. The panel says
  **No message of the day.** when there is no active message.
- Select **REWARDS** to see the current reward status. To claim, open
  **CHAT**, choose **Sign in with Haiven**, and finish signing in through
  the browser. Return to Lyra, open **REWARDS**, and select **CLAIM REWARD**.
  The button appears when the demo has an **Integration ID** and
  **Reward Ref**. The panel shows the result, such as **CLAIMED**,
  **ALREADY CLAIMED**, or **NOT ELIGIBLE**.

## Configuring from the Boss Room environment

The demo reads the same `HAIVEN_DEMO_*` variables as the Boss Room demo, so one
`.env` file configures both. A non-empty variable replaces its Project Settings
value (`[/Script/HaivenLyraDemo.HaivenLyraDemoSettings]` in `DefaultGame.ini`)
for that run; an unset one keeps it.

| Variable | Setting |
| --- | --- |
| `HAIVEN_DEMO_BASE_URL` | `BaseUrl` |
| `HAIVEN_DEMO_REALTIME_URL` | `RealtimeUrl` |
| `HAIVEN_DEMO_APP_ID` | `AppId` |
| `HAIVEN_DEMO_INTEGRATION_ID` | `IntegrationId` |
| `HAIVEN_DEMO_CHANNEL_ID` | `ChannelId` |
| `HAIVEN_DEMO_REWARD_REF` | `RewardRef` |
| `HAIVEN_DEMO_SIGN_IN_URL` (legacy `HAIVEN_DEMO_LOGIN_ENTRY_URL`) | `LoginEntryUrl` |
| `HAIVEN_DEMO_LOGIN_REDIRECT_URL`, else Boss Room's `HAIVEN_DEMO_REDIRECT_URI` | `LoginRedirectUrl` |

`HAIVEN_DEMO_ACCESS_TOKEN` skips the browser sign-in. Without
`HAIVEN_DEMO_ACCESS_TOKEN_EXPIRES_AT` the token is undated and never expires
locally, as in Boss Room; the server still ends it with a 401. Load the file
into the shell that starts the editor:

```sh
set -a; source <path to>/.env; set +a
UnrealEditor "$PWD/Lyra.uproject"
```

`HAIVEN_DEMO_CHAT_HISTORY=true` turns on chat history, the same opt-in as Boss
Room. Lyra then shows the channel's last 20 messages once after the first
connect, oldest first. The Haiven API's game-credential route list does not
include `GET /chat/channels/{channel_id}/messages`, so the browser sign-in's
credential is refused there and no history appears. Turn
`HAIVEN_DEMO_CHAT_HISTORY` on only against a backend that answers that read for
the credential in use.

The message of the day is re-read every 30 seconds and when the game regains
focus. The message stays up while it loads, and a failed read keeps it.

## Match reward

Lyra's counterpart of Boss Room's victory reward: this player's first
elimination in a session claims the configured `RewardRef` once, with the same
reward states as the **Claim reward** button. Signed out, the claim waits until
the player signs in. Lyra broadcasts eliminations on the server, so the reward
fires for the host and for an offline match started with **PLAY**.

<!-- features:start (generated by codegen/features.py; do not edit) -->

### Feature coverage

This table summarizes the SDK used by this demo. The demo exercises the features described in its setup and examples.

| Family | Unreal |
|---|:---:|
| Envelope | ~ |
| Session lifecycle | ~ |
| Request recovery | ✅ |
| Open vocabularies | ~ |
| REST | ○ |
| Game login handoff | ○ |
| P2P sessions | ○ |
| Credential lifecycle | ✅ |
| API surface | ✅ |
| Product | ~ |
| Developer experience | ~ |

✅ shipped · `↑` inherited from the language SDK this target ships, proven only in that SDK's runner and not inside the engine · `~` partial · `—` absent · `○` not yet proven by any scenario · `⊘` not applicable. A family cell is ✅ only when every counted row is ✅, and `↑` when every counted row is ✅ or `↑`; `MAY` requirements and rows marked optional are not counted. Every row: the full Haiven SDK feature matrix.

<!-- features:end -->
