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

1. Fork the repository and branch from `sdk-dev`.
2. Keep the change focused on the Haiven integration unless you are fixing
   a sample file that blocks it.
3. Use a [Conventional Commits](https://www.conventionalcommits.org/)
   subject:

   `type(scope): subject`

   `type` is one of `feat`, `fix`, `docs`, `deps`, `style`, `refactor`,
   `perf`, `test`, `build`, `ci`, `chore`, or `revert`. `scope` is
   optional.
4. Open the pull request against `sdk-dev`. Say what changed and how you
   checked it. Maintainers promote `sdk-dev` into `main`, and release-please
   releases from `main`.

Do not commit access tokens, passwords, private endpoints, or local
credentials.

## Checks

`Lyra.uproject` targets Unreal Engine 5.8. The engine-free checks that
CI runs are:

```sh
python3 -m unittest ci.check_plugins -v
python3 -m unittest ci.test_release -v
```

They do not build or play Lyra. For a change to plugin code, also describe
which editor you opened and what you played. Do not treat a missing
Marketplace content folder as a failure of the Haiven plugins.
