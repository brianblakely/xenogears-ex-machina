//! A bounded history of game runtime states, at acknowledged frame boundaries.
use crate::snapshot::PAGE;
use std::collections::VecDeque;
use std::sync::Arc;
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

// Budget metadata at its 64-bit size on all hosts to retain the same history.
#[repr(C)]
struct Checkpoint {
    frame: u64,
    played: Duration,
    sections: Box<[Section]>,
}

// Align pages to each device or memory region. A change in one region's
// encoded length then leaves all later regions' pages shareable.

struct Section {
    len: u32,
    memory: bool,
    pages: Box<[(u32, Arc<[u8]>)]>,
}

impl Section {
    fn capture(bytes: &[u8], previous: Option<&Section>, memory: bool) -> Self {
        let mut before = previous.into_iter().flat_map(|s| s.pages.iter()).peekable();
        let mut pages = Vec::with_capacity(previous.map_or(0, |s| s.pages.len()));
        for (i, page) in bytes.chunks(PAGE).enumerate() {
            let index = i as u32;
            if before.peek().is_some_and(|(at, _)| *at == index) {
                let (_, old) = before.next().unwrap();
                if **old == *page {
                    pages.push((index, old.clone()));
                    continue;
                }
            }
            if page != &[0; PAGE][..page.len()] { pages.push((index, Arc::from(page))); }
        }
        Self { len: bytes.len() as u32, memory, pages: pages.into_boxed_slice() }
    }

    fn append(&self, out: &mut Vec<u8>) {
        let start = out.len();
        for (index, page) in &self.pages {
            out.resize(start + *index as usize * PAGE, 0);
            out.extend_from_slice(page);
        }
        out.resize(start + self.len as usize, 0);
    }
}

impl Checkpoint {
    fn content(&self) -> Vec<u8> {
        let mut out = crate::snapshot::Writer(Vec::new());
        let mut raw = Vec::new();
        for section in &self.sections {
            if section.memory {
                raw.clear();
                section.append(&mut raw);
                out.memory(&raw);
            } else { section.append(&mut out.0); }
        }
        out.0
    }

    // What this checkpoint alone owns as it joins or leaves the queue.
    fn exclusive_bytes(&self) -> usize {
        self.sections.len() * 24 + self.sections.iter().map(|s| {
            s.pages.len() * 24 + s.pages.iter().map(|(_, p)| p)
                .filter(|p| Arc::strong_count(p) == 1)
                .map(|p| p.len().next_multiple_of(8) + 16).sum::<usize>()
        }).sum::<usize>()
    }
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
    #[cfg(test)]
    fn push(&mut self, frame: u64, data: Vec<u8>, now: Duration) {
        let previous = self.entries.back().and_then(|entry| entry.sections.first());
        self.push_sections(frame, vec![Section::capture(&data, previous, false)].into_boxed_slice(), now);
    }

    pub(crate) fn capture<M: crate::module::GameModule>(&mut self, runtime: &mut crate::Runtime<M>, frame: u64, now: Duration) -> Result<(), crate::snapshot::SnapshotError> {
        let previous = self.entries.back();
        let before = |n| previous.and_then(|entry| entry.sections.get(n));
        let mut regions = Vec::with_capacity(crate::snapshot::REGIONS.len());
        let mut ends = Vec::with_capacity(12);
        let data = runtime.snapshot_content(&mut ends, Some(&mut |raw| {
            regions.push(Section::capture(raw, before(1 + regions.len()), true));
        }))?;
        let mut sections = vec![Section::capture(&data[..ends[0]], before(0), false)];
        sections.append(&mut regions);
        let mut start = ends[0];
        for end in ends[1..].iter().copied().chain([data.len()]) {
            sections.push(Section::capture(&data[start..end], before(sections.len()), false));
            start = end;
        }
        self.push_sections(frame, sections.into_boxed_slice(), now);
        Ok(())
    }

    fn push_sections(&mut self, frame: u64, sections: Box<[Section]>, now: Duration) {
        let entry = Checkpoint { frame, played: self.played, sections };
        self.bytes += entry.exclusive_bytes();
        let capacity = self.entries.capacity();
        self.entries.push_back(entry);
        self.bytes += (self.entries.capacity() - capacity) * 40;
        while self.bytes > self.budget {
            if self.entries.len() < self.entries.capacity() {
                let capacity = self.entries.capacity();
                self.entries.shrink_to_fit();
                self.bytes -= (capacity - self.entries.capacity()) * 40;
                if self.bytes <= self.budget { break; }
            }
            if self.entries.len() == 1 {
                let size = self.bytes;
                self.clear();
                self.error = Some(format!("one state needs {size} bytes; rewind budget is {}", self.budget));
                return;
            }
            self.remove(true);
        }
        self.reset_time(now);
    }

    fn remove(&mut self, oldest: bool) -> Option<Checkpoint> {
        let entry = if oldest { self.entries.pop_front()? } else { self.entries.pop_back()? };
        self.bytes -= entry.exclusive_bytes();
        Some(entry)
    }

    pub fn latest(&self) -> Option<(u64, Vec<u8>)> {
        self.entries.back().map(|entry| (entry.frame,crate::snapshot::seal(entry.content())))
    }
    pub(crate) fn previous(&self, current_frame: u64, steps: usize) -> Option<(u64, Vec<u8>)> {
        let mut index = self.entries.len().checked_sub(1)?;
        if self.entries[index].frame == current_frame { index = index.checked_sub(1)?; }
        index = index.checked_sub(steps.checked_sub(1)?)?;
        let entry = &self.entries[index];
        Some((entry.frame,entry.content()))
    }
    /// The newest state from before frame `before`, taken off the queue
    /// with every later one; play time goes back to the state's own.
    pub(crate) fn pop_before(&mut self, before: u64) -> Option<(u64, Vec<u8>)> {
        let index = self.entries.iter().rposition(|entry| entry.frame < before)?;
        while self.entries.len() > index + 1 { self.remove(false); }
        let entry = self.remove(false)?;
        self.played = entry.played;
        self.last_checkpoint = self.played;
        Some((entry.frame, entry.content()))
    }
    pub fn truncate(&mut self, frame: u64) {
        while self.entries.back().is_some_and(|entry| entry.frame > frame) {
            self.remove(false);
        }
        if let Some(entry) = self.entries.back() {
            self.played = entry.played;
            self.last_checkpoint = self.played;
        }
    }
    fn prune(&mut self) {
        let oldest = self.played.saturating_sub(Duration::from_secs(u64::from(self.history_seconds)));
        if self.entries.front().is_some_and(|entry| entry.played < oldest) {
            while self.entries.front().is_some_and(|entry| entry.played < oldest) { self.remove(true); }
        }
    }
    pub fn available_seconds(&self) -> f64 {
        self.entries.front().map_or(0.0, |entry| self.played.saturating_sub(entry.played).as_secs_f64())
    }
    /// The frame of the oldest state retained, if any.
    pub fn oldest_frame(&self) -> Option<u64> {
        self.entries.front().map(|entry| entry.frame)
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
