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
        ])
        .run(tauri::generate_context!())
        .expect("Doghouse couldn't start");
}
