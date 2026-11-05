# Contributing

Contributions to the Haiven plugins in this repository are welcome:

- `Plugins/Haiven`
- `Plugins/HaivenLyraDemo`
- `Plugins/HaivenLyraInventory`

Lyra is Epic's sample. Keep its license, notices, and existing project
files intact when your change does not require otherwise. Do not add Fab
or Marketplace content.

## Issues

Open a GitHub issue for a bug in the Haiven integration, or for a change
you want to discuss first. Search open issues first.

Include the Unreal Engine version and the steps that show the problem.
Do not paste access tokens, passwords, or private endpoints.

## Pull requests

1. Fork the repository and branch from `main`.
2. Keep the change focused on the Haiven integration unless you are fixing
   a sample file that blocks it.
3. Use a [Conventional Commits](https://www.conventionalcommits.org/)
   subject:

   `type(scope): subject`

   `type` is one of `feat`, `fix`, `docs`, `deps`, `style`, `refactor`,
   `perf`, `test`, `build`, `ci`, `chore`, or `revert`. `scope` is
   optional.
4. Open the pull request against `main`. Say what changed and how you
   checked it.

Do not commit access tokens, passwords, private endpoints, or local
credentials.

## Checks

`Lyra.uproject` targets Unreal Engine 5.8. This repository has no
engine-free test script. Describe which editor you opened and what you
played. Do not treat a missing Marketplace content folder as a failure of
the Haiven plugins.
