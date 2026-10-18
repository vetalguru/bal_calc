# GitHub release

`.github/workflows/release.yml` builds a release when a tag `vX.Y.Z` is
pushed:

- the Android APK, signed with the app's release key
  (`Holdmark-<version>-android-arm64-release.apk`);
- the Windows MSI and the Linux DEB
  (`Holdmark-<version>-windows-x64-release.msi`, `...-linux-amd64-release.deb`).

The workflow first creates a **draft** release whose notes list the changes
since the previous tag; each build then uploads its file straight into it
(release files are not Actions artifacts, so the artifact storage quota does
not apply). You check the draft and publish it.

## Once: the signing secrets

The APK must be signed with the same key as the app on the phones, or an
update will not install over it. The key goes into the repository's
encrypted secrets (Settings → Secrets and variables → Actions), never into
its files. In PowerShell, from the repository folder:

```powershell
# The keystore file, as base64
gh secret set ANDROID_KEYSTORE_BASE64 --body ([Convert]::ToBase64String([IO.File]::ReadAllBytes("$env:USERPROFILE\balcalc-release.keystore")))
gh secret set ANDROID_KEYSTORE_ALIAS --body balcalc
# Asks for the keystore password (it is not shown or saved anywhere else)
gh secret set ANDROID_KEYSTORE_PASSWORD
```

Optional, recommended: the key's certificate fingerprint as a repository
variable. The release then fails if the APK is signed with any other key.

```powershell
keytool -list -v -keystore "$env:USERPROFILE\balcalc-release.keystore" -alias balcalc
# copy the "SHA256:" line's value, then:
gh variable set ANDROID_CERT_SHA256 --body "D2:61:41:...:C7:C0"
```

## Each release

1. Raise the version in the top-level `CMakeLists.txt`
   (`project(ballistics VERSION X.Y.Z ...)`) and `versionCode` in
   `kmp/androidApp/build.gradle.kts`; merge it as "Release: version X.Y.Z".
2. Tag that commit on master and push the tag:

   ```bash
   git checkout master && git pull
   git tag vX.Y.Z
   git push origin vX.Y.Z
   ```

3. Wait for the "Release" workflow (Actions tab, about 15 minutes). It stops
   first if the tag does not match the version in `CMakeLists.txt`.
4. Open the draft under Releases, edit the notes if needed, and press
   Publish release.

A failed build can be re-run from the Actions tab: it keeps the draft and
replaces its own file there. To redo a release, delete
the draft and the tag (`git push --delete origin vX.Y.Z`), then tag again.

The locally signed APK (`tools/android-release.ps1`, see
[android-release.md](android-release.md)) uses the same key and stays the
way to build between releases.
