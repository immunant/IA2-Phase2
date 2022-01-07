use core::arch::asm;

#[repr(C)]
#[derive(Debug, Default, Copy, Clone, PartialEq, Eq)]
pub struct PKRU(i32);

impl PKRU {
    fn read_mask(key: i32) -> i32 {
        1 << (2 * key)
    }
    fn write_mask(key: i32) -> i32 {
        2 << (2 * key)
    }
    pub fn load() -> Self {
        let mut pkru: i32;
        unsafe {
            asm!(
                "rdpkru",
                out("eax") pkru,
                in("ecx") 0,
                out("edx") _,
            );
        }
        Self(pkru)
    }
    pub fn store(self) {
        unsafe {
            asm!(
                "wrpkru",
                in("eax") self.0,
                in("ecx") 0,
                in("edx") 0,
            );
        }
    }
    pub fn can_read(&self, key: i32) -> bool {
        let mask = PKRU::read_mask(key);
        (self.0 & mask) == 0
    }

    pub fn can_write(&self, key: i32) -> bool {
        let mask = PKRU::write_mask(key);
        (self.0 & mask) == 0
    }

    pub fn allow_write(&mut self, key: i32) {
        let mask = PKRU::write_mask(key);
        self.0 &= !mask;
    }

    pub fn forbid_write(&mut self, key: i32) {
        let mask = PKRU::write_mask(key);
        self.0 |= mask;
    }
}
