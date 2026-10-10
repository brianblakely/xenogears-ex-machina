//! A least-recently-used cache of byte buffers bounded by their total size.

use std::collections::{BTreeMap, HashMap};
use std::hash::Hash;

/// Keeps buffers until their total length would exceed the budget, then drops
/// the least recently used. A buffer larger than the whole budget is not kept.
pub struct ByteLru<K> {
    budget: usize,
    used: usize,
    tick: u64,
    entries: HashMap<K, (Vec<u8>, u64)>,
    order: BTreeMap<u64, K>,
}

impl<K: Hash + Eq + Copy> ByteLru<K> {
    pub fn new(budget: usize) -> Self {
        Self {
            budget,
            used: 0,
            tick: 0,
            entries: HashMap::new(),
            order: BTreeMap::new(),
        }
    }

    pub fn budget(&self) -> usize {
        self.budget
    }

    /// Bytes held.
    pub fn used(&self) -> usize {
        self.used
    }

    pub fn len(&self) -> usize {
        self.entries.len()
    }

    pub fn is_empty(&self) -> bool {
        self.entries.is_empty()
    }

    pub fn contains(&self, key: &K) -> bool {
        self.entries.contains_key(key)
    }

    /// The buffer, marked as most recently used.
    pub fn get(&mut self, key: &K) -> Option<&[u8]> {
        let tick = self.next_tick();
        let (data, last) = self.entries.get_mut(key)?;
        self.order.remove(last);
        self.order.insert(tick, *key);
        *last = tick;
        Some(data)
    }

    /// Store a buffer (replacing one under the same key), evicting as needed.
    /// Returns whether it was kept.
    pub fn insert(&mut self, key: K, data: Vec<u8>) -> bool {
        self.remove(&key);
        if data.len() > self.budget {
            return false;
        }
        while self.used + data.len() > self.budget {
            let (_, oldest) = self.order.pop_first().expect("over budget with no entries");
            let (old, _) = self.entries.remove(&oldest).expect("ordered entry");
            self.used -= old.len();
        }
        let tick = self.next_tick();
        self.used += data.len();
        self.order.insert(tick, key);
        self.entries.insert(key, (data, tick));
        true
    }

    pub fn remove(&mut self, key: &K) -> Option<Vec<u8>> {
        let (data, tick) = self.entries.remove(key)?;
        self.order.remove(&tick);
        self.used -= data.len();
        Some(data)
    }

    pub fn clear(&mut self) {
        self.entries.clear();
        self.order.clear();
        self.used = 0;
    }

    fn next_tick(&mut self) -> u64 {
        self.tick += 1;
        self.tick
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn evicts_least_recently_used_within_budget() {
        let mut lru = ByteLru::new(300);
        assert!(lru.insert(1, vec![1; 100]));
        assert!(lru.insert(2, vec![2; 100]));
        assert!(lru.insert(3, vec![3; 100]));
        assert_eq!(lru.used(), 300);
        // Touch 1 so 2 is the oldest.
        assert_eq!(lru.get(&1).unwrap()[0], 1);
        assert!(lru.insert(4, vec![4; 150]));
        assert!(!lru.contains(&2) && !lru.contains(&3));
        assert!(lru.contains(&1) && lru.contains(&4));
        assert_eq!(lru.used(), 250);
        assert!(lru.used() <= lru.budget());
    }

    #[test]
    fn refuses_oversized_and_replaces_same_key() {
        let mut lru = ByteLru::new(100);
        assert!(!lru.insert(1, vec![0; 101]));
        assert!(lru.is_empty());
        assert!(lru.insert(1, vec![0; 60]));
        assert!(lru.insert(1, vec![9; 80]));
        assert_eq!((lru.len(), lru.used()), (1, 80));
        assert_eq!(lru.get(&1).unwrap()[0], 9);
        let mut none = ByteLru::new(0);
        assert!(!none.insert(1, vec![0; 1]));
        assert!(none.insert(2, Vec::new()));
    }
}
