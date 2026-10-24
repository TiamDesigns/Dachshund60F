//! HID devices through hidapi, for the web UI: the app's WebView has no
//! WebHID on macOS and Linux, so boards are listed, opened and written here,
//! and their input reports stream back over a channel. host-lib's
//! tauri-hid.ts is the other end; it decides which devices are boards.

use std::collections::HashMap;
use std::ffi::CString;
use std::sync::atomic::{AtomicBool, AtomicU32, Ordering};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;

use hidapi::{BusType, HidApi, HidDevice};
use serde::Serialize;
use tauri::ipc::Channel;
use tauri::State;

/// A config report plus the report ID hidapi puts first.
const MAX_REPORT: usize = 65;
/// How long the reader sleeps when nothing came in. Writes wait at most this.
const READ_IDLE: Duration = Duration::from_millis(1);

/// One entry of hid_devices, shaped like node-hid's device list.
#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
pub struct DeviceInfo {
    vendor_id: u16,
    product_id: u16,
    path: String,
    serial_number: Option<String>,
    usage_page: u16,
    usage: u16,
    interface: i32,
    /// hidapi's numbering: 1 USB, 2 Bluetooth.
    bus_type: u8,
}

#[derive(Clone, Serialize)]
#[serde(tag = "type", rename_all = "camelCase")]
pub enum HidEvent {
    Report { data: Vec<u8> },
    Closed,
}

struct OpenDevice {
    device: Mutex<HidDevice>,
    closed: AtomicBool,
}

#[derive(Default)]
pub struct Hid {
    /// hidapi allows one instance per process; made on first use.
    api: Mutex<Option<HidApi>>,
    open: Mutex<HashMap<u32, Arc<OpenDevice>>>,
    next: AtomicU32,
}

fn bus_number(bus: BusType) -> u8 {
    match bus {
        BusType::Usb => 1,
        BusType::Bluetooth => 2,
        BusType::I2c => 3,
        BusType::Spi => 4,
        _ => 0,
    }
}

fn with_api<T>(hid: &Hid, f: impl FnOnce(&mut HidApi) -> Result<T, String>) -> Result<T, String> {
    let mut guard = hid.api.lock().map_err(|e| e.to_string())?;
    if guard.is_none() {
        *guard = Some(HidApi::new().map_err(|e| e.to_string())?);
    }
    f(guard.as_mut().unwrap())
}

#[tauri::command(async)]
pub fn hid_devices(hid: State<'_, Hid>) -> Result<Vec<DeviceInfo>, String> {
    with_api(&hid, |api| {
        api.refresh_devices().map_err(|e| e.to_string())?;
        Ok(api
            .device_list()
            .map(|d| DeviceInfo {
                vendor_id: d.vendor_id(),
                product_id: d.product_id(),
                path: d.path().to_string_lossy().into_owned(),
                serial_number: d.serial_number().map(str::to_owned),
                usage_page: d.usage_page(),
                usage: d.usage(),
                interface: d.interface_number(),
                bus_type: bus_number(d.bus_type()),
            })
            .collect())
    })
}

/// Open a device; its reports and its closing arrive on on_event. Returns a handle.
#[tauri::command(async)]
pub fn hid_open(
    hid: State<'_, Hid>,
    path: String,
    on_event: Channel<HidEvent>,
) -> Result<u32, String> {
    let path = CString::new(path).map_err(|e| e.to_string())?;
    let device = with_api(&hid, |api| api.open_path(&path).map_err(|e| e.to_string()))?;
    device.set_blocking_mode(false).map_err(|e| e.to_string())?;
    let open = Arc::new(OpenDevice {
        device: Mutex::new(device),
        closed: AtomicBool::new(false),
    });
    let handle = hid.next.fetch_add(1, Ordering::Relaxed);
    hid.open
        .lock()
        .map_err(|e| e.to_string())?
        .insert(handle, open.clone());

    // Reads don't block, so a write never waits on a read for long.
    thread::spawn(move || {
        let mut buf = [0u8; MAX_REPORT];
        while !open.closed.load(Ordering::Relaxed) {
            let read = match open.device.lock() {
                Ok(device) => device.read(&mut buf),
                Err(_) => break,
            };
            match read {
                Ok(0) => thread::sleep(READ_IDLE),
                Ok(n) => {
                    if on_event
                        .send(HidEvent::Report {
                            data: buf[..n].to_vec(),
                        })
                        .is_err()
                    {
                        break; // the page went away
                    }
                }
                Err(_) => break, // unplugged
            }
        }
        open.closed.store(true, Ordering::Relaxed);
        let _ = on_event.send(HidEvent::Closed);
    });
    Ok(handle)
}

/// Write one output report, report ID first. Returns the bytes written.
#[tauri::command(async)]
pub fn hid_write(hid: State<'_, Hid>, handle: u32, data: Vec<u8>) -> Result<usize, String> {
    if data.is_empty() || data.len() > MAX_REPORT {
        return Err(format!("a report is 1 to {MAX_REPORT} bytes"));
    }
    let open = hid
        .open
        .lock()
        .map_err(|e| e.to_string())?
        .get(&handle)
        .cloned();
    let open = open.ok_or("device not open")?;
    if open.closed.load(Ordering::Relaxed) {
        return Err("device closed".into());
    }
    let device = open.device.lock().map_err(|e| e.to_string())?;
    device.write(&data).map_err(|e| e.to_string())
}

/// Close a device. Its reader stops and sends Closed.
#[tauri::command(async)]
pub fn hid_close(hid: State<'_, Hid>, handle: u32) -> Result<(), String> {
    if let Some(open) = hid.open.lock().map_err(|e| e.to_string())?.remove(&handle) {
        open.closed.store(true, Ordering::Relaxed);
    }
    Ok(())
}
