% Calibrated computational listeners for the Binaural sound mode
% (docs/runtime.md, Binaural sound mode; plan.md Phase 8).
%
% The renderer's measured binaural impulse responses (xem-binaural's
% rendered_hrirs example) are heard by the Auditory Modeling Toolbox's
% localization models in place of people: baumgartner2014 for the sagittal
% planes (quadrant errors, local polar error, front/back confusions) with
% the 23 listeners of its pool, each through their own measured ears and
% the sensitivity fitted to their own localization experiments, and with
% the KEMAR head itself as a listener (the pool's median sensitivity);
% dietz2011 with an ITD-to-angle lookup from the same ears for the lateral
% angle. Each hears three versions of the same directions: the renderer's
% responses, the KEMAR set's raw responses they come from, and as a control
% the original Stereo pan law over headphones. barumerli2023 is reported
% as not run: it cannot run under Octave (models.json's `barumerli2023`
% says why, in Octave's and the toolbox's own words).
%
% The toolchain is checked first: the baumgartner2014 pool's predictions
% for its own ears must reproduce the published ones (Baumgartner, Majdak
% and Laback 2014, Table 1), or nothing else runs.
%
%   cargo run --release -p xem-binaural --example rendered_hrirs
%   nix develop path:./nix/amt -c octave-cli --no-init-file tools/binaural_models/evaluate.m
%
% from the repository's root (the AMT is GPL-3.0+, a local test tool: see
% nix/amt/flake.nix). Writes build/binaural/models.json and prints a
% summary; computed lookups and lateral estimates are kept in
% build/binaural/models-cache (every noise comes from a fixed state of the
% generator, so a run with an empty cache computes the same). Exits with
% status 1 when an acceptance check fails: the rendered responses heard
% within the models' noise of the raw ones (pool median differences within
% 2 points of quadrant error and of front/back confusion, 2 degrees of
% polar and of lateral error), and Binaural confused front and back less
% often than the Stereo control.

1; % a script with functions

function start_amt()
  amt = getenv('XEM_AMT');
  if isempty(amt)
    error('XEM_AMT is not set: run inside nix develop path:./nix/amt');
  end
  % On the path before amt_start, which otherwise finds them itself and
  % saves the path to ~/.octaverc.
  thirdparty = {'ltfat/ltfat-2.6.0', 'sofa/SOFAtoolbox', 'sfs/sfs-matlab-2.5.0', ...
                'circstat', 'binauralSH/BinauralSH-main', 'bads/bads-1.1.2'};
  for t = thirdparty
    addpath(fullfile(amt, 'thirdparty', t{1}));
  end
  addpath(amt);
  % 'localonly': everything is in the store; nothing is downloaded.
  amt_start('silent', 'localonly');
end

% A set of the measurement: its responses (direction x ear x sample) and
% directions (azimuth clockwise from ahead, elevation; degrees).
function set = read_set(out, description, name)
  sets = description.sets;
  if ~iscell(sets)
    sets = num2cell(sets);
  end
  for i = 1:numel(sets)
    if strcmp(sets{i}.name, name)
      set = sets{i};
      fid = fopen(fullfile(out, set.file), 'r', 'ieee-le');
      x = fread(fid, Inf, 'single=>double');
      fclose(fid);
      m = size(set.directions, 1);
      set.ir = permute(reshape(x, set.length, 2, m), [3 2 1]);
      set.md5 = file_md5(fullfile(out, set.file));
      return;
    end
  end
  error('no set %s in hrirs.json', name);
end

function h = file_md5(path)
  fid = fopen(path, 'r');
  bytes = fread(fid, Inf, 'uint8=>char')';
  fclose(fid);
  h = hash('md5', bytes);
end

% SOFA's azimuth turns counter-clockwise (90 is the left).
function Obj = to_sofa(ir, directions, fs)
  Obj = SOFAgetConventions('SimpleFreeFieldHRIR');
  Obj.Data.IR = ir;
  Obj.Data.SamplingRate = fs;
  m = size(directions, 1);
  Obj.SourcePosition = [mod(-directions(:, 1), 360), directions(:, 2), ones(m, 1)];
  Obj = SOFAupdateDimensions(Obj);
end

% A direction's lateral angle (positive to the left, SOFA's way) and polar
% angle (0 ahead, 90 above, 180 behind), degrees, from the renderer's
% azimuth and elevation.
function [lat, pol] = interaural(directions)
  az = -directions(:, 1) * pi / 180;
  el = directions(:, 2) * pi / 180;
  x = cos(el) .* cos(az); y = cos(el) .* sin(az); z = sin(el);
  lat = asin(y) * 180 / pi;
  pol = atan2(z, x) * 180 / pi;
  pol(pol < -90) = pol(pol < -90) + 360;
end

% Front/back confusions: of the targets at least 30 degrees from the
% frontal plane (polar angles up to 60 or from 120), the probability of a
% response on the other side of it (polar angle 90), on average, percent.
function fb = front_back(p, tang, rang)
  p = p ./ repmat(sum(p, 1), size(p, 1), 1);
  rang = rang(:);
  front = rang < 90;
  back = rang > 90;
  confused = [];
  for i = 1:numel(tang)
    if tang(i) <= 60
      confused(end + 1) = sum(p(back, i));
    elseif tang(i) >= 120
      confused(end + 1) = sum(p(front, i));
    end
  end
  fb = 100 * mean(confused);
end

% The sagittal-plane predictions of baumgartner2014 for `target` heard
% through `template`'s ears at sensitivity S, in each plane of `lats`:
% quadrant error (percent), local polar RMS error (degrees) and
% front/back confusions (percent), each plane's targets weighted alike.
function r = sagittal(target, template, S, lats)
  r.lat = lats;
  for l = 1:numel(lats)
    [p, rang, tang] = baumgartner2014(target, template, 'S', S, 'lat', lats(l));
    [r.qe(l), r.pe(l)] = baumgartner2014_pmv2ppp(p, tang, rang);
    r.fb(l) = front_back(p, tang, rang);
    r.targets(l) = numel(tang);
  end
  r.mean_qe = mean(r.qe);
  r.mean_pe = mean(r.pe);
  r.mean_fb = mean(r.fb);
end

% The ears at 44.1 kHz (the pool's are measured at 48 kHz).
function Obj = at_44k(Obj)
  if Obj.Data.SamplingRate == 44100
    return;
  end
  [p, q] = rat(44100 / Obj.Data.SamplingRate);
  ir = Obj.Data.IR;
  first = resample(squeeze(ir(1, 1, :)), p, q);
  out = zeros(size(ir, 1), size(ir, 2), numel(first));
  for m = 1:size(ir, 1)
    for e = 1:size(ir, 2)
      out(m, e, :) = resample(squeeze(ir(m, e, :)), p, q);
    end
  end
  Obj.Data.IR = out;
  Obj.Data.SamplingRate = 44100;
  Obj = SOFAupdateDimensions(Obj);
end

% The ITD-to-angle lookup of a listener's ears (itd2angle_lookuptable,
% dietz2011), kept in the cache under `key`. The toolbox makes it from a
% second of white noise it draws itself (LTFAT's noise, that is randn):
% the generator is set to `state` first, so every lookup hears the same
% noise and a run with an empty cache makes the same lookups.
function lookup = lookup_of(Obj, key, cache, state)
  path = fullfile(cache, ['lookup-' key '.mat']);
  if exist(path, 'file')
    load(path, 'lookup');
    return;
  end
  randn('state', state);
  lookup = itd2angle_lookuptable(at_44k(Obj), 44100, 'dietz2011');
  save('-binary', path, 'lookup');
end

% The lateral angle each lookup gives for a binaural signal: what
% wierstorf2013_estimateazimuth(sig, lookup, 'fs', fs) gives (dietz2011
% below 1.4 kHz, interaural coherence above 0.98 and rising, the median of
% each channel, channels further than 30 degrees from their median left
% out, no spectral weighting), its model run once for all the lookups.
function phi = lateral(sig, fs, lookups)
  [fine, ~, ild] = dietz2011(sig, fs, 'nolowpass', 'fhigh', 1400);
  itd = dietz2011_unwrapitd(fine.itd, ild, fine.f_inst, 2.5);
  phi = nan(1, numel(lookups));
  for l = 1:numel(lookups)
    angles = itd2angle(itd, lookups{l});
    azimuth = nan(1, size(angles, 2));
    for n = 1:size(angles, 2)
      chosen = fine.ic(:, n) > 0.98 & [diff(fine.ic(:, n)) > 0; 0];
      a = angles(chosen, n);
      a = a(~isnan(a));
      if ~isempty(a)
        azimuth(n) = median(a);
      end
    end
    azimuth = azimuth(~isnan(azimuth));
    if ~isempty(azimuth)
      azimuth = azimuth(abs(azimuth - median(azimuth)) < 30);
      phi(l) = median(azimuth);
    end
  end
end

% Each direction of a set heard by every lookup (direction x lookup),
% kept in the cache for the set's file, the noise and the lookups' names.
function phi = lateral_set(set, noise, fs, lookups, names, cache)
  key = hash('md5', [set.md5 sprintf('%s,', names{:}) sprintf('%.17g,', noise(1:16))]);
  path = fullfile(cache, ['lateral-' set.name '-' key '.mat']);
  if exist(path, 'file')
    load(path, 'phi');
    return;
  end
  m = size(set.ir, 1);
  phi = nan(m, numel(lookups));
  for d = 1:m
    sig = [fftfilt(squeeze(set.ir(d, 1, :)), noise), fftfilt(squeeze(set.ir(d, 2, :)), noise)];
    phi(d, :) = lateral(sig, fs, lookups);
  end
  save('-binary', path, 'phi');
end

% Lateral RMS error and bias (positive: further out than the target, for
% targets at least 10 degrees to a side), degrees, over the directions
% `kept`.
function r = lateral_errors(phi, lat, kept)
  e = phi(kept) - lat(kept);
  r.rms = sqrt(mean(e .^ 2));
  side = kept & abs(lat) >= 10;
  r.bias = mean(sign(lat(side)) .* (phi(side) - lat(side)));
  r.compared = sum(kept);
  r.unestimated = sum(isnan(phi));
end

% What the rendered responses keep of the raw ones' cues, direction by
% direction: the interaural delay (broadband cross-correlation, to an
% eighth of a sample), the broadband level difference between the ears,
% each ear's spectral shape from 700 Hz to 16 kHz (the standard deviation
% over frequency of the level difference, its mean removed) and that mean
% (the renderer's levelling).
function c = cues(rendered, raw, fs)
  n = 8192;
  f = (0:n / 2)' * fs / n;
  band = f > 700 & f < 16000;
  m = size(rendered, 1);
  c = struct('delay', zeros(m, 1), 'ild', zeros(m, 1), 'shape', zeros(m, 2), 'gain', zeros(m, 2));
  delay = @(ir) local_delay(squeeze(ir(1, 1, :)), squeeze(ir(1, 2, :)), n);
  for d = 1:m
    a = rendered(d, :, :); b = raw(d, :, :);
    c.delay(d) = delay(a) - delay(b);
    level = @(ir, e) 10 * log10(sum(squeeze(ir(1, e, :)) .^ 2));
    c.ild(d) = (level(a, 1) - level(a, 2)) - (level(b, 1) - level(b, 2));
    for e = 1:2
      A = abs(fft(squeeze(a(1, e, :)), n)); B = abs(fft(squeeze(b(1, e, :)), n));
      diff_db = 20 * log10(A(1:n / 2 + 1)(band)) - 20 * log10(B(1:n / 2 + 1)(band));
      c.gain(d, e) = mean(diff_db);
      c.shape(d, e) = std(diff_db, 1);
    end
  end
end

% The delay of the left ear behind the right (samples, to an eighth).
function lag = local_delay(left, right, n)
  up = 8;
  L = fft(left, n); R = fft(right, n);
  X = L .* conj(R);
  padded = [X(1:n / 2); zeros((up - 1) * n, 1); X(n / 2 + 1:end)];
  x = real(ifft(padded));
  span = 40 * up;
  window = [x(end - span + 1:end); x(1:span + 1)];
  [~, i] = max(window);
  lag = (i - span - 1) / up;
end

function s = spread(x)
  x = sort(x(:));
  s = struct('median', median(x), 'q25', x(max(1, round(0.25 * numel(x)))), ...
             'q75', x(min(numel(x), round(0.75 * numel(x)))), 'min', x(1), 'max', x(end));
end

% --------------------------------------------------------------------------

% Stopped, Octave would otherwise save its whole workspace (hundreds of
% megabytes) into the current directory.
crash_dumps_octave_core(false);
here = fileparts(mfilename('fullpath'));
root = fileparts(fileparts(here));
out = fullfile(root, 'build', 'binaural');
cache = fullfile(out, 'models-cache');
if ~exist(cache, 'dir')
  mkdir(cache);
end
start_amt();
fid = fopen(fullfile(getenv('XEM_AMT'), 'amt_version'));
amt = strtrim(fread(fid, Inf, 'char=>char')');
fclose(fid);
results = struct();
results.toolchain = struct('octave', version(), 'amt', amt, 'ltfat', ltfathelp('version'), 'sofa', SOFAgetVersion());
printf('Octave %s, AMT %s, LTFAT %s, SOFA Toolbox %s\n', version(), amt, ltfathelp('version'), SOFAgetVersion());

% 1. The toolchain: the pool's own-ear predictions against the published
% ones. Baumgartner, Majdak and Laback (2014), "Modeling sound-source
% localization in sagittal planes for human listeners", JASA 136(2):791-802,
% Table 1: listener, targets N, sensitivity S, quadrant errors actual and
% predicted (percent), local polar RMS errors actual and predicted
% (degrees), lateral angles within 30 degrees of the median plane.
published = [
  12 1506 0.26  2.19  3.28 27.7 26.9
  14  140 0.58  5.00  4.42 25.5 26.1
  15  996 0.55  2.51  5.76 33.3 30.0
  16  960 0.63  5.83  8.00 31.9 28.9
  17  364 0.76  7.69  8.99 33.8 32.1
  18  310 1.05 20.0  20.0  36.4 36.4
  21  291 0.71  9.62 10.0  34.0 33.3
  22  266 0.70 10.2  10.3  33.6 33.4
  33  275 0.88 17.1  17.8  35.7 34.4
  39  484 0.86 10.7  12.0  37.4 35.5
  41  264 1.02 18.9  17.7  37.1 39.7
  42  300 0.44  3.67  6.20 30.0 27.1
  43  127 0.44  1.57  6.46 34.0 28.0
  46  127 0.46  3.94  4.78 28.5 27.5
  53  164 0.52  1.83  3.42 26.5 24.9
  55  123 0.88  9.76 12.6  38.1 33.4
  57  119 0.97 19.3  16.8  28.0 33.4
  58  153 0.21  1.96  2.75 24.5 23.8
  62  282 0.98 11.3  13.2  38.6 35.5
  64  306 0.84  9.48  9.68 33.5 33.1
  68  269 0.76 11.9  11.7  32.4 32.9
  71  104 0.76  9.62  9.32 33.1 33.5
  72  304 0.79 10.9  12.6  38.0 35.3
];
printf('\n1. Toolchain check: baumgartner2014, the pool through its own ears\n');
baseline = data_baumgartner2014('baseline');
segments = [-20 0 20];
check = zeros(numel(baseline), 9);
for ll = 1:numel(baseline)
  row = published(published(:, 1) == str2double(baseline(ll).id(3:end)), :);
  n = zeros(1, 3); targets = cell(1, 3); planes = cell(1, 3); angles = cell(1, 3);
  for ii = 1:3
    responses = baseline(ll).itemlist(:, 7);
    in = responses <= segments(ii) + 10 & responses > segments(ii) - 10;
    targets{ii} = real(baseline(ll).itemlist(in, 6));
    n(ii) = numel(targets{ii});
    [planes{ii}, angles{ii}] = extractsp(segments(ii), baseline(ll).Obj);
  end
  predicted = zeros(2, 2);
  for which = 1:2
    S = baseline(ll).S;
    if which == 2
      S = row(3);
    end
    qe = zeros(1, 3); pe = zeros(1, 3);
    for ii = 1:3
      [p, rang] = baumgartner2014(planes{ii}, planes{ii}, baseline(ll).fs, 'S', S, 'lat', segments(ii), 'polsamp', angles{ii});
      [qe(ii), pe(ii)] = baumgartner2014_pmv2ppp(p, angles{ii}, rang, targets{ii});
    end
    w = n / sum(n);
    predicted(which, :) = [sum(w .* qe), sum(w .* pe)];
  end
  check(ll, :) = [row(1), sum(n), baseline(ll).S, baseline(ll).qe_exp, baseline(ll).pe_exp, predicted(1, :), predicted(2, :)];
  printf('  NH%d: N %d, S %.3f (published %.2f); actual QE %.2f%% PE %.1f; predicted QE %.2f%% PE %.1f with the toolbox''s S, %.2f%% %.1f with the published S (published %.2f%% %.1f)\n', ...
         row(1), sum(n), baseline(ll).S, row(3), baseline(ll).qe_exp, baseline(ll).pe_exp, predicted(1, 1), predicted(1, 2), predicted(2, 1), predicted(2, 2), row(5), row(7));
end
ids = check(:, 1);
rows = arrayfun(@(id) find(published(:, 1) == id), ids);
pub = published(rows, :);
deviation = struct( ...
  'targets', max(abs(check(:, 2) - pub(:, 2))), ...
  'sensitivity', max(abs(check(:, 3) - pub(:, 3))), ...
  'actual_qe', max(abs(check(:, 4) - pub(:, 4))), ...
  'actual_pe', max(abs(check(:, 5) - pub(:, 6))), ...
  'predicted_qe', max(abs(check(:, 6) - pub(:, 5))), ...
  'predicted_pe', max(abs(check(:, 7) - pub(:, 7))), ...
  'predicted_qe_published_s', max(abs(check(:, 8) - pub(:, 5))), ...
  'predicted_pe_published_s', max(abs(check(:, 9) - pub(:, 7))));
valid = deviation.targets == 0 && deviation.sensitivity <= 0.015 && deviation.actual_qe <= 0.051 ...
        && deviation.actual_pe <= 0.051 && max([deviation.predicted_qe, deviation.predicted_qe_published_s]) <= 0.5 ...
        && max([deviation.predicted_pe, deviation.predicted_pe_published_s]) <= 0.5;
results.validation = struct('source', 'Baumgartner, Majdak and Laback (2014), JASA 136(2):791-802, Table 1', ...
  'listeners', numel(baseline), 'deviation', deviation, 'passed', valid, ...
  'tolerance', 'targets equal, S within 0.015, actual errors as rounded, predictions within 0.5 points and 0.5 degrees', ...
  'rows', {num2cell(check, 2)});
printf('  worst differences from Table 1: targets %d, S %.3f, actual QE %.3f PE %.3f; predicted QE %.2f points, PE %.2f degrees (with the published S: %.2f, %.2f)\n', ...
       deviation.targets, deviation.sensitivity, deviation.actual_qe, deviation.actual_pe, deviation.predicted_qe, deviation.predicted_pe, ...
       deviation.predicted_qe_published_s, deviation.predicted_pe_published_s);
if ~valid
  fid = fopen(fullfile(out, 'models.json'), 'w'); fputs(fid, jsonencode(results)); fclose(fid);
  error('the toolchain does not reproduce the published predictions: nothing else is run');
end
printf('  passed\n');

% 2. The measured sets. baumgartner2014 takes each response's band levels
% over the response's own length (the RMS of its gammatone-filtered
% samples), so a response shorter than its low bands' filters ring (the
% KEMAR set's 128 samples) has those bands cut short, more or less by
% where its onset falls. Every set is heard at the rendered responses'
% length: the shorter ones zero-padded, which is what they are beyond
% their samples.
description = jsondecode(fileread(fullfile(out, 'hrirs.json')));
fs = description.sample_rate;
names = {'rendered-measured', 'kemar-raw', 'stereo-measured', 'rendered-grid', 'rendered-music', 'rendered-upmix'};
sets = struct();
for i = 1:numel(names)
  sets.(strrep(names{i}, '-', '_')) = read_set(out, description, names{i});
end
common = max(structfun(@(s) size(s.ir, 3), sets));
for f = fieldnames(sets)'
  s = sets.(f{1});
  % As measured, for the lateral lookup (which takes medians over time:
  % padding would add silence to them).
  s.native = to_sofa(s.ir, s.directions, fs);
  s.ir(:, :, end + 1:common) = 0;
  s.sofa = to_sofa(s.ir, s.directions, fs);
  sets.(f{1}) = s;
end
compared = {'rendered_measured', 'kemar_raw', 'stereo_measured', 'rendered_grid'};
kept = cues(sets.rendered_measured.ir, sets.kemar_raw.ir, fs);
shapes = sort(kept.shape(:));
results.cues = struct('directions', size(kept.delay, 1), 'delay_worst', max(abs(kept.delay)), ...
  'ild_median', median(abs(kept.ild)), 'ild_worst', max(abs(kept.ild)), 'shape_median', median(shapes), ...
  'shape_p95', shapes(round(0.95 * numel(shapes))), 'shape_worst', shapes(end), 'gain_median', median(kept.gain(:)), ...
  'gain_min', min(kept.gain(:)), 'gain_max', max(kept.gain(:)));
printf(['\n2. Cues at the %d measured directions, rendered against raw: interaural delay within %.3f samples; ' ...
        'level difference between the ears within %.2f dB (median %.3f); each ear''s spectral shape 0.7-16 kHz ' ...
        'within %.2f dB (standard deviation over frequency; median %.2f, 95%% %.2f); the levelling''s gain %.1f to %+.1f dB (median %.1f)\n'], ...
       results.cues.directions, results.cues.delay_worst, results.cues.ild_worst, results.cues.ild_median, results.cues.shape_worst, ...
       results.cues.shape_median, results.cues.shape_p95, results.cues.gain_min, results.cues.gain_max, results.cues.gain_median);

% 3. The listeners: the pool's 23 through their own ears at their own
% sensitivity (the toolbox's calibration, as data_baumgartner2014 gives
% it), and the KEMAR head at the pool's median sensitivity.
pool = data_baumgartner2014('pool');
listeners = struct('name', {}, 'Obj', {}, 'ears', {}, 'S', {});
for ll = 1:numel(pool)
  listeners(end + 1) = struct('name', pool(ll).id, 'Obj', pool(ll).Obj, 'ears', pool(ll).Obj, 'S', pool(ll).S);
end
kemar_s = median([pool.S]);
listeners(end + 1) = struct('name', 'KEMAR', 'Obj', sets.kemar_raw.sofa, 'ears', sets.kemar_raw.native, 'S', kemar_s);
printf('\n3. Sagittal planes (baumgartner2014), lateral angles -30 to 30 in steps of 10; %d listeners (KEMAR at S %.3f)\n', numel(listeners), kemar_s);
lats = -30:10:30;
sag = struct();
for t = 1:numel(listeners)
  for g = 1:numel(compared)
    r = sagittal(sets.(compared{g}).sofa, listeners(t).Obj, listeners(t).S, lats);
    sag.(compared{g})(t) = r;
  end
  printf('  %-5s rendered QE %5.1f%% PE %4.1f FB %5.1f%% | raw %5.1f%% %4.1f %5.1f%% | stereo %5.1f%% %4.1f %5.1f%% | grid %5.1f%% %4.1f %5.1f%%\n', ...
         listeners(t).name, sag.rendered_measured(t).mean_qe, sag.rendered_measured(t).mean_pe, sag.rendered_measured(t).mean_fb, ...
         sag.kemar_raw(t).mean_qe, sag.kemar_raw(t).mean_pe, sag.kemar_raw(t).mean_fb, ...
         sag.stereo_measured(t).mean_qe, sag.stereo_measured(t).mean_pe, sag.stereo_measured(t).mean_fb, ...
         sag.rendered_grid(t).mean_qe, sag.rendered_grid(t).mean_pe, sag.rendered_grid(t).mean_fb);
end

% 4. Lateral angles (dietz2011 with each listener's ITD-to-angle lookup):
% half a second of white noise through each direction's responses. The
% lookups' noise and this noise come from fixed states of the generator,
% different ones, so the lookups are not made from the noise they hear.
printf('\n4. Lateral angles (dietz2011 and an ITD-to-angle lookup from each listener''s ears)\n');
lookup_state = 2011;
lookups = cell(1, numel(listeners));
lookup_names = cell(1, numel(listeners));
for t = 1:numel(listeners)
  ears = listeners(t).ears;
  lookup_names{t} = sprintf('%s-%d-%d-state%d', listeners(t).name, ears.Data.SamplingRate, size(ears.Data.IR, 3), lookup_state);
  lookups{t} = lookup_of(ears, lookup_names{t}, cache, lookup_state);
end
randn('state', 2014);
noise = randn(round(0.5 * fs), 1);
lat_sets = {'rendered_measured', 'kemar_raw', 'stereo_measured', 'rendered_music', 'rendered_upmix'};
phi = struct();
for g = 1:numel(lat_sets)
  tic;
  phi.(lat_sets{g}) = lateral_set(sets.(lat_sets{g}), noise, fs, lookups, lookup_names, cache);
  printf('  %s: %d directions (%.0f s)\n', lat_sets{g}, size(phi.(lat_sets{g}), 1), toc);
end
% The estimate for one direction agrees with the toolbox's own function.
probe = sets.rendered_measured;
d = find(probe.directions(:, 1) == 30 & probe.directions(:, 2) == 0);
sig = [fftfilt(squeeze(probe.ir(d, 1, :)), noise), fftfilt(squeeze(probe.ir(d, 2, :)), noise)];
own = wierstorf2013_estimateazimuth(sig, lookups{end}, 'fs', fs);
if abs(own - phi.rendered_measured(d, end)) > 1e-9
  error('the lateral estimate (%.4f) is not wierstorf2013_estimateazimuth''s (%.4f)', phi.rendered_measured(d, end), own);
end
% The raw set's median-plane responses have identical ears (its left half
% mirrors the right, and the median plane is its own mirror): the signal
% is the same at both ears, its interaural coherence never rises, and the
% model's gate takes no sample, so there is no estimate. The rendered
% responses there differ by a little noise and get one. Rendered and raw
% are compared over the directions both have estimates for; the control
% over its own.
both = ~isnan(phi.rendered_measured) & ~isnan(phi.kemar_raw);
lat_errors = struct();
for g = 1:3
  [lat_true, ~] = interaural(sets.(lat_sets{g}).directions);
  for t = 1:numel(listeners)
    kept = both(:, t);
    if g == 3
      kept = ~isnan(phi.(lat_sets{g})(:, t));
    end
    lat_errors.(lat_sets{g})(t) = lateral_errors(phi.(lat_sets{g})(:, t), lat_true, kept);
  end
end
% A lookup gives no angle beyond 95 degrees (itd2angle), so how many
% directions have an estimate can also differ from listener to listener.
counts = @(x) sprintf('%d to %d', min(x), max(x));
printf('  compared, over the listeners: rendered and raw over %s directions (no estimate: raw %s, rendered %s), the control over %s\n', ...
       counts(sum(both)), counts(sum(isnan(phi.kemar_raw))), counts(sum(isnan(phi.rendered_measured))), ...
       counts(sum(~isnan(phi.stereo_measured))));
for t = 1:numel(listeners)
  printf('  %-5s rendered RMS %5.1f bias %+5.1f | raw %5.1f %+5.1f | stereo %5.1f %+5.1f\n', listeners(t).name, ...
         lat_errors.rendered_measured(t).rms, lat_errors.rendered_measured(t).bias, lat_errors.kemar_raw(t).rms, ...
         lat_errors.kemar_raw(t).bias, lat_errors.stereo_measured(t).rms, lat_errors.stereo_measured(t).bias);
end

% 5. The arrangement's places and the estimate's loudspeakers: where the
% pool hears each (polar errors and front/back from baumgartner2014 in the
% place's own sagittal plane, the lateral angle from dietz2011).
printf('\n5. The music''s places and the estimate''s loudspeakers, as the pool hears them (medians over the 23)\n');
places = struct('set', {}, 'label', {}, 'azimuth', {}, 'elevation', {}, 'lateral', {}, 'polar', {}, ...
                'heard_lateral', {}, 'qe', {}, 'pe', {}, 'other_side', {});
for g = {'rendered_music', 'rendered_upmix'}
  s = sets.(g{1});
  labels = s.labels;
  [lat_true, pol_true] = interaural(s.directions);
  for d = 1:size(s.directions, 1)
    one = to_sofa(s.ir(d, :, :), s.directions(d, :), fs);
    qe = nan(1, numel(pool)); pe = qe; other = qe;
    % The sagittal model's planes reach 60 degrees to a side (the pool's
    % experiments' lateral range); further out only the lateral angle is told.
    if abs(lat_true(d)) <= 60
      for t = 1:numel(pool)
        [p, rang, tang] = baumgartner2014(one, listeners(t).Obj, 'S', listeners(t).S, 'lat', lat_true(d));
        [qe(t), pe(t)] = baumgartner2014_pmv2ppp(p, tang, rang);
        p = p / sum(p);
        if tang <= 60
          other(t) = 100 * sum(p(rang > 90));
        elseif tang >= 120
          other(t) = 100 * sum(p(rang < 90));
        end
      end
    end
    places(end + 1) = struct('set', g{1}, 'label', labels{d}, 'azimuth', s.directions(d, 1), 'elevation', s.directions(d, 2), ...
                             'lateral', lat_true(d), 'polar', pol_true(d), 'heard_lateral', median(phi.(g{1})(d, 1:numel(pool))), ...
                             'qe', median(qe), 'pe', median(pe), 'other_side', median(other));
    printf('  %-12s %6.1f/%4.1f: lateral %+6.1f heard %+6.1f; polar %5.1f: QE %5.1f%% PE %4.1f, other side of the frontal plane %5.1f%%\n', ...
           labels{d}, s.directions(d, 1), s.directions(d, 2), lat_true(d), places(end).heard_lateral, pol_true(d), ...
           places(end).qe, places(end).pe, places(end).other_side);
  end
end

% 6. barumerli2023 under Octave.
lastwarn('');
barumerli2023('sofa_obj', sets.kemar_raw.sofa);
[refusal, ~] = lastwarn();
try
  barumerli2023_coordinates([0 0 1], 'spherical');
  coordinates = 'constructed';
catch problem
  coordinates = problem.message;
end
% The class's base, alone: a class deriving from it and nothing else.
probe = fullfile(cache, 'copyable');
if ~exist(probe, 'dir')
  mkdir(probe);
end
fid = fopen(fullfile(probe, 'copyable_probe.m'), 'w');
fputs(fid, sprintf('classdef copyable_probe < matlab.mixin.Copyable\nend\n'));
fclose(fid);
addpath(probe);
try
  copyable_probe();
  copyable = 'constructed';
catch problem
  copyable = problem.message;
end
rmpath(probe);
results.barumerli2023 = struct('ran', false, 'refusal', refusal, 'coordinates_class', coordinates, 'copyable', copyable, ...
  'reason', ['AMT 1.6.0 returns from barumerli2023 and barumerli2023_featureextraction under Octave with a warning; both build ' ...
             'their directions with barumerli2023_coordinates, a MATLAB handle class deriving from matlab.mixin.Copyable with ' ...
             'typed properties, which Octave cannot parse or provide (AMT ticket 345)']);
printf('\n6. barumerli2023: not run. The toolbox: "%s"; its coordinates class: "%s"; matlab.mixin.Copyable: "%s"\n', refusal, coordinates, copyable);

% 7. Acceptance, relative.
n = numel(pool);
pool_of = @(r, field) arrayfun(@(x) x.(field), r(1:n));
kemar_of = @(r, field) r(end).(field);
summary = struct();
fails = {};
for field = {'mean_qe', 'mean_pe', 'mean_fb'}
  f = field{1};
  for g = {'rendered_measured', 'kemar_raw', 'stereo_measured', 'rendered_grid'}
    summary.sagittal.(g{1}).(f) = spread(pool_of(sag.(g{1}), f));
    summary.sagittal.(g{1}).([f '_kemar']) = kemar_of(sag.(g{1}), f);
  end
  delta = pool_of(sag.rendered_measured, f) - pool_of(sag.kemar_raw, f);
  summary.sagittal.rendered_minus_raw.(f) = spread(delta);
  summary.sagittal.rendered_minus_raw.([f '_kemar']) = kemar_of(sag.rendered_measured, f) - kemar_of(sag.kemar_raw, f);
  summary.sagittal.grid_minus_measured.(f) = spread(pool_of(sag.rendered_grid, f) - pool_of(sag.rendered_measured, f));
  if abs(median(delta)) > 2 || abs(summary.sagittal.rendered_minus_raw.([f '_kemar'])) > 2
    fails{end + 1} = sprintf('sagittal %s: rendered against raw, pool median %+.2f, KEMAR %+.2f', f, median(delta), ...
                             summary.sagittal.rendered_minus_raw.([f '_kemar']));
  end
end
for field = {'rms', 'bias'}
  f = field{1};
  for g = {'rendered_measured', 'kemar_raw', 'stereo_measured'}
    summary.lateral.(g{1}).(f) = spread(pool_of(lat_errors.(g{1}), f));
    summary.lateral.(g{1}).([f '_kemar']) = kemar_of(lat_errors.(g{1}), f);
  end
  delta = pool_of(lat_errors.rendered_measured, f) - pool_of(lat_errors.kemar_raw, f);
  summary.lateral.rendered_minus_raw.(f) = spread(delta);
  summary.lateral.rendered_minus_raw.([f '_kemar']) = kemar_of(lat_errors.rendered_measured, f) - kemar_of(lat_errors.kemar_raw, f);
  if abs(median(delta)) > 2 || abs(summary.lateral.rendered_minus_raw.([f '_kemar'])) > 2
    fails{end + 1} = sprintf('lateral %s: rendered against raw, pool median %+.2f, KEMAR %+.2f', f, median(delta), ...
                             summary.lateral.rendered_minus_raw.([f '_kemar']));
  end
end
binaural_fb = pool_of(sag.rendered_measured, 'mean_fb');
stereo_fb = pool_of(sag.stereo_measured, 'mean_fb');
summary.front_back = struct('binaural', spread(binaural_fb), 'stereo', spread(stereo_fb), ...
  'listeners_better', sum(binaural_fb < stereo_fb), 'listeners', n, ...
  'kemar_binaural', kemar_of(sag.rendered_measured, 'mean_fb'), 'kemar_stereo', kemar_of(sag.stereo_measured, 'mean_fb'));
if ~(median(binaural_fb) < median(stereo_fb)) || ~(summary.front_back.kemar_binaural < summary.front_back.kemar_stereo)
  fails{end + 1} = sprintf('front/back: Binaural %.1f%% against Stereo %.1f%% (pool medians)', median(binaural_fb), median(stereo_fb));
end
results.listeners = {listeners.name};
results.sensitivities = [listeners.S];
results.lateral_planes = lats;
results.sagittal = sag;
results.lateral = lat_errors;
results.places = places;
results.summary = summary;
results.acceptance = struct('passed', isempty(fails), 'failures', {fails}, ...
  'criteria', ['rendered against raw KEMAR: pool median and KEMAR-listener differences within 2 points (quadrant error, ' ...
               'front/back confusions) and 2 degrees (local polar error, lateral RMS error and bias); Binaural confuses ' ...
               'front and back less than the Stereo control (pool median and the KEMAR listener)']);
fid = fopen(fullfile(out, 'models.json'), 'w');
fputs(fid, jsonencode(results));
fclose(fid);

m = @(x) x.median;
printf('\n7. Summary (pool medians; the KEMAR listener in brackets)\n');
printf('  sagittal QE: rendered %.1f%% (%.1f), raw %.1f%% (%.1f), stereo %.1f%% (%.1f), grid %.1f%%\n', ...
       m(summary.sagittal.rendered_measured.mean_qe), summary.sagittal.rendered_measured.mean_qe_kemar, ...
       m(summary.sagittal.kemar_raw.mean_qe), summary.sagittal.kemar_raw.mean_qe_kemar, ...
       m(summary.sagittal.stereo_measured.mean_qe), summary.sagittal.stereo_measured.mean_qe_kemar, m(summary.sagittal.rendered_grid.mean_qe));
printf('  sagittal PE: rendered %.1f (%.1f), raw %.1f (%.1f), stereo %.1f (%.1f), grid %.1f degrees\n', ...
       m(summary.sagittal.rendered_measured.mean_pe), summary.sagittal.rendered_measured.mean_pe_kemar, ...
       m(summary.sagittal.kemar_raw.mean_pe), summary.sagittal.kemar_raw.mean_pe_kemar, ...
       m(summary.sagittal.stereo_measured.mean_pe), summary.sagittal.stereo_measured.mean_pe_kemar, m(summary.sagittal.rendered_grid.mean_pe));
printf('  front/back: rendered %.1f%% (%.1f), raw %.1f%% (%.1f), stereo %.1f%% (%.1f); Binaural below Stereo for %d of %d listeners\n', ...
       m(summary.sagittal.rendered_measured.mean_fb), summary.sagittal.rendered_measured.mean_fb_kemar, ...
       m(summary.sagittal.kemar_raw.mean_fb), summary.sagittal.kemar_raw.mean_fb_kemar, ...
       m(summary.sagittal.stereo_measured.mean_fb), summary.sagittal.stereo_measured.mean_fb_kemar, summary.front_back.listeners_better, n);
printf('  rendered minus raw: QE %+.2f points (%+.2f), PE %+.2f degrees (%+.2f), FB %+.2f points (%+.2f)\n', ...
       m(summary.sagittal.rendered_minus_raw.mean_qe), summary.sagittal.rendered_minus_raw.mean_qe_kemar, ...
       m(summary.sagittal.rendered_minus_raw.mean_pe), summary.sagittal.rendered_minus_raw.mean_pe_kemar, ...
       m(summary.sagittal.rendered_minus_raw.mean_fb), summary.sagittal.rendered_minus_raw.mean_fb_kemar);
printf('  lateral RMS: rendered %.1f (%.1f), raw %.1f (%.1f), stereo %.1f (%.1f) degrees; bias rendered %+.1f, raw %+.1f, stereo %+.1f\n', ...
       m(summary.lateral.rendered_measured.rms), summary.lateral.rendered_measured.rms_kemar, ...
       m(summary.lateral.kemar_raw.rms), summary.lateral.kemar_raw.rms_kemar, ...
       m(summary.lateral.stereo_measured.rms), summary.lateral.stereo_measured.rms_kemar, ...
       m(summary.lateral.rendered_measured.bias), m(summary.lateral.kemar_raw.bias), m(summary.lateral.stereo_measured.bias));
printf('  lateral rendered minus raw: RMS %+.2f (%+.2f), bias %+.2f (%+.2f) degrees\n', ...
       m(summary.lateral.rendered_minus_raw.rms), summary.lateral.rendered_minus_raw.rms_kemar, ...
       m(summary.lateral.rendered_minus_raw.bias), summary.lateral.rendered_minus_raw.bias_kemar);
if isempty(fails)
  printf('\nAcceptance: passed. Wrote %s\n', fullfile(out, 'models.json'));
else
  printf('\nAcceptance: FAILED\n');
  printf('  %s\n', fails{:});
  exit(1);
end
