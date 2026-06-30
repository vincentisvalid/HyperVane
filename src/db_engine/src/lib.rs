pub mod db;
pub mod hash;
pub mod ipc;
pub mod search;

pub mod proto {
    include!(concat!(env!("OUT_DIR"), "/hypervane.rs"));
}
