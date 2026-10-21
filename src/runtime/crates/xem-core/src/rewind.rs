//! A bounded history of game runtime states, at acknowledged frame boundaries.
use std::collections::VecDeque;
use std::time::Duration;

/// The most the retained states may take, whatever the history length.
pub const MAX_REWIND_BYTES: usize = 2 * 1024 * 1024 * 1024;
/// A held rewind takes one state off the queue this often.
pub const REWIND_STEP_MS: u64 = 250;
/// Vertical blanks a held rewind plays on from each state at most: the
/// display shows a frame both built and drawn after the state from its
/// third change of buffers (three to six blanks where the game draws every
/// other one, up to twelve where it draws every fourth).
pub const REWIND_PLAY_BLANKS: u64 = 12;

// Give checkpoint metadata the same actual allocation layout on supported
// 32/64-bit hosts, so one memory budget retains the same history on each.
#[repr(C, align(32))]
struct Checkpoint {
    frame: u64,
    played: Duration,
    // Serialized game save content. The file checksum is only needed when a
    // state leaves this trusted queue; computing it here stalls the frame.
    data: Vec<u8>,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn history_expires_by_elapsed_play_time_and_rewind_moves_its_time_back() {
        let mut history = Rewind::default();
        history.configure(4096,1,crate::time::DEFAULT_REWIND_HISTORY_SECONDS,Duration::ZERO).unwrap();
        for second in 0..=30 {
            let now = Duration::from_secs(second);
            assert!(history.due(now));
            history.push(second*120,vec![second as u8],now);
        }
        assert_eq!(history.status()["states"],31);
        assert_eq!(history.status()["oldest_frame"],0);
        assert_eq!(history.available_seconds(),30.0);
        assert!(history.due(Duration::from_secs(31)));
        history.push(31*120,vec![31],Duration::from_secs(31));
        assert_eq!(history.status()["oldest_frame"],120);
        assert_eq!(history.status()["states"],31);
        history.reset_time(Duration::from_secs(3600));
        assert!(!history.due(Duration::from_millis(3600500)),"paused time cannot age out game history");
        assert!(history.due(Duration::from_secs(3601)));
        history.push(32*120,vec![32],Duration::from_secs(3601));
        assert_eq!(history.status()["oldest_frame"],240);
        let (frame,state) = history.previous(32*120,3).unwrap();
        assert_eq!((frame,state),(29*120,vec![29]));
        history.truncate(frame);
        assert_eq!(history.available_seconds(),27.0);
        history.configure(4096,1,5,Duration::from_secs(3601)).unwrap();
        assert_eq!(history.status()["oldest_frame"],24*120);
        assert_eq!(history.status()["states"],6);
        assert_eq!(history.available_seconds(),5.0);
        history.configure(4096,1,30,Duration::from_secs(3601)).unwrap();
        assert_eq!(history.status()["states"],6,"increasing retention preserves the remaining queue");
    }

    #[test]
    fn a_held_rewind_pops_each_state_once_and_keeps_its_play_time() {
        let mut history = Rewind::default();
        history.configure(4096,1,30,Duration::ZERO).unwrap();
        for second in 0..5 {
            history.due(Duration::from_secs(second));
            history.push(second*60,vec![second as u8],Duration::from_secs(second));
        }
        // The game at frame 241 (a state at 240): the newest one before it.
        assert_eq!(history.pop_before(241),Some((240,vec![4])));
        assert_eq!(history.status()["states"],4);
        // Played on from it (frame 242 now), the next step goes further back.
        assert_eq!(history.pop_before(242),Some((180,vec![3])));
        assert_eq!(history.available_seconds(),3.0,"play time is the popped state's own");
        // A state no earlier than the game is skipped and dropped.
        assert_eq!(history.pop_before(60),Some((0,vec![0])));
        assert_eq!(history.status()["states"],0);
        assert_eq!(history.pop_before(60),None,"the beginning of history");
    }
}

#[derive(Default)]
pub struct Rewind {
    pub budget: usize,
    pub interval_seconds: u32,
    pub history_seconds: u32,
    pub bytes: usize,
    pub error: Option<String>,
    entries: VecDeque<Checkpoint>,
    last_checkpoint: Duration,
    played: Duration,
    clock_at: Duration,
}

impl Rewind {
    pub fn configure(&mut self, budget: usize, interval_seconds: u32, history_seconds: u32, now: Duration) -> Result<(), String> {
        if budget > MAX_REWIND_BYTES || !(1..=crate::time::MAX_REWIND_INTERVAL_SECONDS).contains(&interval_seconds)
            || !(interval_seconds..=crate::time::MAX_REWIND_HISTORY_SECONDS).contains(&history_seconds) {
            return Err(format!("rewind needs at most {MAX_REWIND_BYTES} bytes, and 1 <= interval <= history <= {} seconds",crate::time::MAX_REWIND_HISTORY_SECONDS));
        }
        if budget != self.budget { self.clear(); self.error = None; }
        self.budget = budget;
        self.interval_seconds = interval_seconds;
        self.history_seconds = history_seconds;
        self.prune();
        self.reset_time(now);
        Ok(())
    }
    pub fn reset_time(&mut self, now: Duration) {
        self.clock_at = now;
        self.last_checkpoint = self.played;
    }
    pub fn due(&mut self, now: Duration) -> bool {
        self.played += now.saturating_sub(self.clock_at);
        self.clock_at = now;
        self.prune();
        self.budget != 0 && self.error.is_none() && (self.entries.is_empty()
            || self.played.saturating_sub(self.last_checkpoint) >= Duration::from_secs(u64::from(self.interval_seconds)))
    }
    pub(crate) fn push(&mut self, frame: u64, data: Vec<u8>, now: Duration) {
        // Include both the actual state allocation and container capacity.
        let size = data.capacity() + std::mem::size_of::<Checkpoint>();
        if size > self.budget {
            self.clear();
            self.error = Some(format!("one state needs {size} bytes; rewind budget is {}", self.budget));
            return;
        }
        while self.bytes + size > self.budget {
            let old = self.entries.pop_front().unwrap();
            self.bytes -= old.data.capacity() + std::mem::size_of::<Checkpoint>();
        }
        self.entries.reserve_exact(1);
        self.entries.push_back(Checkpoint { frame, played: self.played, data });
        self.reset_time(now);
        self.recount();
        while self.bytes > self.budget {
            self.entries.pop_front();
            self.recount();
        }
    }
    pub fn latest(&self) -> Option<(u64, Vec<u8>)> {
        self.entries.back().map(|entry| (entry.frame,crate::snapshot::seal(entry.data.clone())))
    }
    pub(crate) fn previous(&self, current_frame: u64, steps: usize) -> Option<(u64, Vec<u8>)> {
        let mut index = self.entries.len().checked_sub(1)?;
        if self.entries[index].frame == current_frame { index = index.checked_sub(1)?; }
        index = index.checked_sub(steps.checked_sub(1)?)?;
        let entry = &self.entries[index];
        Some((entry.frame,entry.data.clone()))
    }
    /// The newest state from before frame `before`, taken off the queue
    /// with every later one; play time goes back to the state's own.
    pub(crate) fn pop_before(&mut self, before: u64) -> Option<(u64, Vec<u8>)> {
        let index = self.entries.iter().rposition(|entry| entry.frame < before)?;
        self.entries.truncate(index + 1);
        let entry = self.entries.pop_back()?;
        self.played = entry.played;
        self.last_checkpoint = self.played;
        self.recount();
        Some((entry.frame, entry.data))
    }
    pub fn truncate(&mut self, frame: u64) {
        while self.entries.back().is_some_and(|entry| entry.frame > frame) {
            self.entries.pop_back();
        }
        if let Some(entry) = self.entries.back() {
            self.played = entry.played;
            self.last_checkpoint = self.played;
        }
        self.recount();
    }
    fn prune(&mut self) {
        let oldest = self.played.saturating_sub(Duration::from_secs(u64::from(self.history_seconds)));
        if self.entries.front().is_some_and(|entry| entry.played < oldest) {
            while self.entries.front().is_some_and(|entry| entry.played < oldest) { self.entries.pop_front(); }
            self.recount();
        }
    }
    pub fn available_seconds(&self) -> f64 {
        self.entries.front().map_or(0.0, |entry| self.played.saturating_sub(entry.played).as_secs_f64())
    }
    /// The frame of the oldest state retained, if any.
    pub fn oldest_frame(&self) -> Option<u64> {
        self.entries.front().map(|entry| entry.frame)
    }
    fn recount(&mut self) {
        self.entries.shrink_to_fit();
        self.bytes = self.entries.iter().map(|entry| entry.data.capacity()).sum::<usize>()
            + self.entries.capacity() * std::mem::size_of::<Checkpoint>();
    }
    pub fn clear(&mut self) {
        self.entries = VecDeque::new();
        self.bytes = 0;
        self.played = Duration::ZERO;
        self.last_checkpoint = Duration::ZERO;
    }
    pub fn status(&self) -> serde_json::Value {
        serde_json::json!({ "max_bytes": self.budget, "bytes": self.bytes, "interval_seconds": self.interval_seconds,
            "history_seconds": self.history_seconds, "available_seconds": self.available_seconds(),
            "states": self.entries.len(), "oldest_frame": self.oldest_frame(),
            "newest_frame": self.entries.back().map(|s| s.frame), "error": self.error })
    }
}
