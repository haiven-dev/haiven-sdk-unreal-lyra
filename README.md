Lyra Sample Game
===

Content Information
----
When downloading Lyra Source code from git, the content folders are not included.

To make use of the Lyra source code, you will need to download the content from the Unreal Marketplace through the Epic Games Launcher.

For more information please visit the [Unreal Engine Lyra documentation](https://docs.unrealengine.com/5.0/en-US/lyra-sample-game-in-unreal-engine/)

Once installed, copy the "Content" folder and any other folders named "Content" from the Marketplace project you downloaded to the project in your solution, and you should be good to go.


## Haiven versioning

[haiven-sdk-unreal](https://github.com/haiven-dev/haiven-sdk-unreal) follows semver: a feature ships in a minor release and a fix ships in a patch.

The policy for this demo is to track the major.minor of haiven-sdk-unreal. A feature that shows part of the platform in Lyra ships as a demo patch, with no change to haiven-sdk-unreal. A breaking change that only affects this demo also ships as a demo patch, not as a minor or major release.

The release configuration does not enforce this policy yet. Today release-please versions this repository on its own from conventional commits ([release-please-config.json](https://github.com/haiven-dev/haiven-sdk-unreal-lyra/blob/main/release-please-config.json)), starting at 0.0.1, and nothing ties this version to haiven-sdk-unreal. Before 1.0, `bump-patch-for-minor-pre-major` turns a `feat:` commit into a patch bump, which matches the policy. But `bump-minor-pre-major` turns a breaking commit (`feat!:` or a `BREAKING CHANGE:` footer) into a minor bump, which does not. Until the configuration changes, add a `Release-As: x.y.z` footer that names the next patch version to the body of a demo-only breaking commit. release-please then uses that version instead of the minor bump. Pull requests into `sdk-dev` are squash-merged and release-please reads the squash commit, so the `Release-As:` line must be the last line of the squash commit message: make the breaking commit the pull request's last commit, or edit the message when you merge.

release-please writes this repository's version to `VersionName` in the HaivenLyraDemo and HaivenLyraInventory plugin descriptors, and `ci/release.py` checks a release tag against them. `Plugins/Haiven/Haiven.uplugin` is a copy of the haiven-sdk-unreal plugin, so its `VersionName` is the SDK version from the last plugin sync, and neither release-please nor `ci/release.py` changes or compares it.


Additional Information
----
## Licensing and contributing

Lyra's Marketplace content is not in this git checkout, and there is no
`../../../README.md` here. The Epic sample is covered by the Unreal Engine
license that came with that Marketplace download. See the
[Lyra sample documentation](https://docs.unrealengine.com/5.0/en-US/lyra-sample-game-in-unreal-engine/).

Haiven changes in this repository are the plugins
[Haiven](Plugins/Haiven/Haiven.uplugin),
[HaivenLyraDemo](Plugins/HaivenLyraDemo/HaivenLyraDemo.uplugin), and
[HaivenLyraInventory](Plugins/HaivenLyraInventory/HaivenLyraInventory.uplugin).
Review those as GitHub pull requests. `.github/workflows/build.yml` checks the
three descriptors on pull requests and on a `v*` tag. It does not cook Lyra;
that still needs the Marketplace content and an Unreal install.

## Playing

Press Play on the front end, then press **PLAY** on the left. That hosts Lyra's
default experience as a local match, without an Epic lobby, so the shooter can
be played the way Boss Room can be played after Host. Chat and rewards stay on
edge handles and do not take the mouse. Without the Fab content, PLAY reports
that no gameplay experience is loaded.

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

`HAIVEN_DEMO_CHAT_HISTORY=true` shows the channel's last 20 messages once after
the first connect, oldest first, as Boss Room's chat history opt-in does.

The message of the day is re-read every 30 seconds and when the game regains
focus. The message stays up while it loads, and a failed read keeps it.

## Match reward

Lyra's counterpart of Boss Room's victory reward: this player's first
elimination in a session claims the configured `RewardRef` once, with the same
reward states as the **Claim reward** button. Signed out, the claim waits until
the player signs in. Lyra broadcasts eliminations on the server, so the reward
fires for the host and for an offline match started with **PLAY**.
