//! Doghouse as a desktop app: the web UI from app/ in a native window, plus
//! what a browser can't do everywhere: HID (hid.rs), writing firmware to a
//! board's UF2 drive (uf2.rs) and updating itself.

mod hid;
mod uf2;

/// Release builds add the updater's public key to the config (see
/// app/src-tauri/README.md); without one there are no updates to check.
#[tauri::command]
fn updates_enabled(app: tauri::AppHandle) -> bool {
    app.config().plugins.0.contains_key("updater")
}

/// Where people download Doghouse by hand (app/src/lib/app-version.ts).
const RELEASES_URL: &str = "https://github.com/TiamDesigns/Dachshund60F/releases/latest";

/// Opens the download page in the computer's browser, for copies that can't
/// install an update themselves. Only this one address, never one from the UI.
#[tauri::command]
fn open_releases_page() -> Result<(), String> {
    #[cfg(target_os = "windows")]
    let mut command = {
        let mut c = std::process::Command::new("rundll32");
        c.args(["url.dll,FileProtocolHandler", RELEASES_URL]);
        c
    };
    #[cfg(target_os = "macos")]
    let mut command = {
        let mut c = std::process::Command::new("open");
        c.arg(RELEASES_URL);
        c
    };
    #[cfg(not(any(target_os = "windows", target_os = "macos")))]
    let mut command = {
        let mut c = std::process::Command::new("xdg-open");
        c.arg(RELEASES_URL);
        c
    };
    command
        .spawn()
        .map(|_| ())
        .map_err(|err| format!("couldn't open the browser: {err}"))
}

pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_process::init())
        .manage(hid::Hid::default())
        .setup(|app| {
            if app.config().plugins.0.contains_key("updater") {
                app.handle()
                    .plugin(tauri_plugin_updater::Builder::new().build())?;
            }
            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            hid::hid_devices,
            hid::hid_open,
            hid::hid_write,
            hid::hid_close,
            uf2::uf2_drives,
            uf2::uf2_write,
            updates_enabled,
            open_releases_page,
        ])
        .run(tauri::generate_context!())
        .expect("Doghouse couldn't start");
}
