/// Levenshtein-distance score normalised to [0.0, 1.0].
/// 1.0 = exact match, 0.0 = completely different.
pub fn fuzzy_rank(query: &str, candidate: &str) -> f32 {
    let q: Vec<char> = query.chars().collect();
    let c: Vec<char> = candidate.chars().collect();
    let dist = levenshtein(&q, &c);
    let max_len = q.len().max(c.len());
    if max_len == 0 {
        return 1.0;
    }
    1.0 - (dist as f32 / max_len as f32)
}

fn levenshtein(a: &[char], b: &[char]) -> usize {
    let m = a.len();
    let n = b.len();
    let mut dp = vec![vec![0usize; n + 1]; m + 1];
    for i in 0..=m { dp[i][0] = i; }
    for j in 0..=n { dp[0][j] = j; }
    for i in 1..=m {
        for j in 1..=n {
            dp[i][j] = if a[i - 1] == b[j - 1] {
                dp[i - 1][j - 1]
            } else {
                1 + dp[i - 1][j].min(dp[i][j - 1]).min(dp[i - 1][j - 1])
            };
        }
    }
    dp[m][n]
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn exact_match_is_one() {
        assert!((fuzzy_rank("sonic", "sonic") - 1.0).abs() < f32::EPSILON);
    }

    #[test]
    fn empty_strings_are_one() {
        assert!((fuzzy_rank("", "") - 1.0).abs() < f32::EPSILON);
    }

    #[test]
    fn close_match_is_high() {
        assert!(fuzzy_rank("mario", "morio") > 0.7);
    }
}
