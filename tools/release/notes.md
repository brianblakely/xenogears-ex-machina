Welcome to Xenogears Ex Machina {tag}! Pick the download for your device below, or [play in your browser]({browser}).

## Installation

You'll need images of your own North American Xenogears discs (`SLUS-00664` and `SLUS-00669`). CHD, BIN/CUE, and raw 2352-byte ISO images work. If you have CUE sheets, keep them with their track files. You don't need a BIOS.

- **Windows:** Download the [Windows ZIP]({downloads}windows-x86_64.zip) and extract it into a folder you can save files in. Keep the included files together and open `xem-desktop.exe`.
- **macOS:** Download the ZIP for [Apple Silicon]({downloads}macos-aarch64.zip) or [Intel]({downloads}macos-x86_64.zip), extract it, and open `Xenogears Ex Machina.app`.
- **Linux:** Download the AppImage for [most PCs (x86-64)]({downloads}linux-x86_64.AppImage) or [ARM devices (AArch64)]({downloads}linux-aarch64.AppImage). Make it executable with `chmod +x filename.AppImage`, then run it. If it won't start, the [Linux setup guide]({guide}#desktop-installation) covers the permissions it needs.
- **Android / Meta Horizon OS:** Download and install the [APK]({downloads}android-horizon.apk), then choose your local disc images when the app asks for them.
- **Browser:** Open [Xenogears Ex Machina]({browser}) and import your disc images. They stay on your device. Export a save backup before clearing your browser's storage.

**Playing on desktop?** Put your disc images beside the executable, AppImage, or `.app` bundle. On first launch, the app sorts them into `discs/`. Your memory cards go in `cards/`, and save states go in `saves/`.

### Steam Deck & Steam Machine

Using SteamOS? You can launch the Linux app straight from your Steam library:

1. Switch to Desktop Mode and create a Xenogears folder inside your home folder.
2. Save the [Linux x86-64 AppImage]({downloads}linux-x86_64.AppImage) in that folder. Right-click it, open **Properties → Permissions**, and mark it as executable. Put your disc images in the same folder.
3. Open the AppImage once to finish disc setup. In Steam, choose **Games → Add a Non-Steam Game to My Library**, browse to the AppImage, and add it.
4. Return to Gaming Mode and open the game from your library's **Non-Steam** tab.

When you download a newer version, update the Steam shortcut to point to the new AppImage and keep your disc and save folders. Valve's [desktop guide](https://help.steampowered.com/en/faqs/view/671A-4453-E8D2-323C) has more help with Steam's menus.

### Updating

**Updating an existing installation?** Back up your game folder, then replace the app and its included files. Keep `discs/`, `cards/`, `saves/`, and `settings.json`. On Android and Meta Horizon OS, install the update over the existing app. Memory cards are the best way to carry progress between versions; save states may need the same version that created them.

Need a hand? The [installation guide]({guide}) has more setup help and the current limitations.

## What's new

{changelog}
