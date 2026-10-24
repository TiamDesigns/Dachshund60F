//! A board's UF2 drive, for firmware updates: the WebView has no drive
//! picker outside Chrome and Edge, so the app finds the mounted drives that
//! hold an INFO_UF2.TXT and writes the file onto the one the web UI names.

use std::fs;
use std::io::Write;
use std::path::{Path, PathBuf};

use serde::Serialize;

/// UF2 blocks are 512 bytes and start with these two words (little endian).
const BLOCK: usize = 512;
const MAGIC: [u8; 8] = [0x55, 0x46, 0x32, 0x0a, 0x57, 0x51, 0x5d, 0x9e];
/// INFO_UF2.TXT is a few lines; anything longer isn't one.
const INFO_MAX: u64 = 4096;

#[derive(Serialize)]
pub struct Uf2Drive {
    /// Where it's mounted.
    path: String,
    /// The contents of its INFO_UF2.TXT (Board-ID and so on).
    info: String,
}

/// Places a removable drive may be mounted.
fn mount_points() -> Vec<PathBuf> {
    if cfg!(windows) {
        // A: and B: are floppy letters; touching them can stall.
        (b'C'..=b'Z')
            .map(|c| PathBuf::from(format!("{}:\\", c as char)))
            .collect()
    } else if cfg!(target_os = "macos") {
        fs::read_dir("/Volumes")
            .map(|d| d.flatten().map(|e| e.path()).collect())
            .unwrap_or_default()
    } else {
        // FAT file systems from /proc/self/mounts; spaces come escaped as \040.
        let mounts = fs::read_to_string("/proc/self/mounts").unwrap_or_default();
        mounts
            .lines()
            .filter_map(|line| {
                let mut fields = line.split(' ');
                let (_, dir, kind) = (fields.next()?, fields.next()?, fields.next()?);
                matches!(kind, "vfat" | "msdos" | "fat")
                    .then(|| PathBuf::from(dir.replace("\\040", " ")))
            })
            .collect()
    }
}

fn info_of(root: &Path) -> Option<String> {
    let file = root.join("INFO_UF2.TXT");
    if fs::metadata(&file).ok()?.len() > INFO_MAX {
        return None;
    }
    fs::read_to_string(file).ok()
}

fn drives() -> Vec<Uf2Drive> {
    mount_points()
        .into_iter()
        .filter_map(|root| {
            let info = info_of(&root)?;
            Some(Uf2Drive {
                path: root.to_string_lossy().into_owned(),
                info,
            })
        })
        .collect()
}

#[tauri::command(async)]
pub fn uf2_drives() -> Vec<Uf2Drive> {
    drives()
}

/// Write a UF2 image onto a drive from uf2_drives (and nowhere else).
#[tauri::command(async)]
pub fn uf2_write(drive: String, data: Vec<u8>) -> Result<(), String> {
    if data.is_empty() || data.len() % BLOCK != 0 || !data.chunks(BLOCK).all(|b| b[..8] == MAGIC) {
        return Err("that isn't a UF2 file".into());
    }
    if !drives().iter().any(|d| d.path == drive) {
        return Err("that drive isn't a UF2 drive any more".into());
    }
    let mut file =
        fs::File::create(Path::new(&drive).join("FIRMWARE.UF2")).map_err(|e| e.to_string())?;
    // The bootloader restarts once the last block lands, so the end of this
    // can fail; the board coming back is the real test.
    file.write_all(&data).map_err(|e| e.to_string())?;
    let _ = file.sync_all();
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn finds_a_uf2_drive_in_a_folder() {
        let dir = std::env::temp_dir().join(format!("doghouse-uf2-{}", std::process::id()));
        fs::create_dir_all(&dir).unwrap();
        assert_eq!(info_of(&dir), None);
        fs::write(
            dir.join("INFO_UF2.TXT"),
            "Board-ID: nRF52840-Dachshund60F-v1\n",
        )
        .unwrap();
        assert_eq!(
            info_of(&dir).as_deref(),
            Some("Board-ID: nRF52840-Dachshund60F-v1\n")
        );
        fs::remove_dir_all(&dir).unwrap();
    }

    #[test]
    fn refuses_what_isnt_uf2() {
        assert!(uf2_write("/nowhere".into(), vec![]).is_err());
        assert!(uf2_write("/nowhere".into(), vec![0; BLOCK]).is_err());
        let mut block = vec![0u8; BLOCK];
        block[..8].copy_from_slice(&MAGIC);
        // A real image, but not onto a drive from uf2_drives.
        assert_eq!(
            uf2_write("/nowhere".into(), block).unwrap_err(),
            "that drive isn't a UF2 drive any more"
        );
    }
}
