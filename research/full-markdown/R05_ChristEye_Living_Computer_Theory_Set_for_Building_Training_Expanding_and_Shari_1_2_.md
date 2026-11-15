> **Source manuscript** · Extracted from original non-patent research archive · © 2026 We Develop Software LLC.

# ChristEye Living Computer: Theory Set for Building, Training, Expanding and Sharing

Nov 13, 2026 · @Brad

## 1. Scope and evidence labels

This is a complete, testable theory set for the ChristEye living computer, but it is a set of hypotheses and measurement rules, not a set of discoveries. Nothing here has been run on living tissue. Your own Design Specification v1.0 says the same, and this document keeps that boundary.

Every equation carries one of three labels:

| Label | Meaning | Examples |
| --- | --- | --- |
| **E** Established | Standard physics, statistics or published method | Data rate, energy balance, membrane equation, power analysis |
| **D** Derived here | A definition I wrote to fill a gap your documents left open | Retention index R\_U, teaching effect, Lotus decision rule |
| **H** Hypothesis | A claim that only an experiment can settle | Astrocyte contribution, support-network benefit, anything about transfer |

**How the Six Stars outputs are used.** The Atlas records 95 answered prompts from persona prompts running on a small language model. Those answers are generated text. They are not observations of any cell, and a language model cannot discover a biological fact by reasoning in character. The Atlas itself says its equations are analyst formalizations of the prose, and that the corpus does not establish Spirit = astrocyte or memory transfer. I treat the Star relations the same way: as hypothesis seeds. Where a seed was sound, I tightened it into a measurable form and cite its Atlas ID.

**What I read.** In full: Design Specification v1.0 and the Framework article v1.3.5. In part: Technical Companion sections 7 to 10 and Appendix B (the formal models), and the Atlas scorecard, equation table and boundaries. The patent I saw only at heading level; it covers the software inquiry engine, so little of this document depends on it.

**Out of scope.** Wet-lab recipes, cell handling steps and anything involving a person or animal. Those belong to a qualified laboratory under institutional biosafety review, as section 10 of your specification requires.

## 2. Vocabulary to variables

Each ChristEye term maps to one symbol with a unit and an instrument. A term with no measurable symbol cannot appear in a claim.

| Term | Symbol | What is measured | Unit |
| --- | --- | --- | --- |
| Soul | Ω | The declared whole: one cartridge, its boundary and observation window | identifier + hours |
| Spirits (neurons) | x(t) | Spike counts per electrode per time bin | spikes per bin |
| Astrocytic state | a(t) | Calcium-event or reporter statistics, where imaged | events per minute |
| Soil | z(t) | Receiving conditions: pH, osmolality, temperature, glucose, lactate | native units |
| Milk | q(t) | Support adequacy: are all Soil variables inside locked bounds | 0 to 1 |
| Honey | m(t) | Outcome-contingent feedback signal delivered by the host | signed, dimensionless |
| Mesh | M, W | Typed edges; W is the evoked-response influence matrix | dimensionless gain |
| Roots | R | Performance that survives a delay with feedback off | 0 to 1 |
| Tree | G\_T | Rooted record of which retained relations support which tasks | graph |
| Wind | y(t) | Decoded output delivered to a named recipient | class label + confidence |
| Understanding | U = (R, G, F, C) | Retention, transfer, flexibility, calibration (Companion L6) | four numbers, never summed |
| Lotus | π | Host coordinator policy: ACT, HOLD or UNRESOLVED | decision |
| Heart | g\_j | Independent limit controller: hard constraints with veto | pass or veto |
| Marrow | θ\_0 | Source lineage record and release data | record |
| Astroengram | H | Hypothesis that a(t) adds task-specific information beyond x(t) | ΔL, section 6 |

Two rules follow from your own documents. Understanding stays a vector, because a single score would let accuracy hide a calibration failure. Milk never depends on task score, so q(t) is a safety variable and not a reward.

## 3. Core state model

The whole system reduces to five coupled relations: network dynamics, astrocytic state, bounded plasticity, support, and observed performance. The first three come from Companion L1 to L4; the last two fill gaps.

**3.1 Network dynamics (H, Companion L4).** Activity at the next bin depends on recurrent influence, the encoded stimulus c, astrocytic state a and Soil z.

```latex
\mathbf{x}_{t+1} = f\left(W\mathbf{x}_t + B\mathbf{c}_t + G\mathbf{a}_t + D\mathbf{z}_t\right)
```

**3.2 Astrocytic state (H, Companion L3).** Slower than neurons, driven by them and by Soil.

```latex
\boldsymbol{\tau}_a \odot \frac{d\mathbf{a}}{dt} = -\mathbf{a} + F(\mathbf{x}, \mathbf{z}, \mathbf{a})
```

**3.3 Bounded change (H, Companion L1 and L2).** A weight changes only when three things coincide: local eligibility e, Honey m, and Milk q. Decay pulls it back toward baseline.

```latex
\tau_e \frac{de_{ij}}{dt} = -e_{ij} + \Phi(x_i, x_j, \Delta t), \qquad \frac{dw_{ij}}{dt} = \eta\, m(t)\, q(t)\, e_{ij} - \lambda\,(w_{ij} - w_{ij}^{0})
```

This is the Milk and Honey process in one line. Honey selects, Milk permits, and neither alone changes anything. The product η·m·q cannot be separated from weight data alone, so m and q must each be measured independently.

**3.4 Milk as a range, not a dose (D).** The Companion says support must be an appropriate range and leaves the form open. The simplest form that respects that:

```latex
q(t) = \prod_{k} \exp\left[-\frac{1}{2}\left(\frac{\max(0,\; l_k - z_k,\; z_k - u_k)}{s_k}\right)^{2}\right]
```

q equals 1 while every Soil variable z\_k sits inside its locked band \[l\_k, u\_k\] and falls smoothly outside it. More of anything never raises q above 1. The bands and widths s\_k are qualification data, not free parameters.

**3.5 Roots times access (D, from Atlas ROOTS-01, ROOTS-04, WATER-01).** Three Stars independently proposed that expressed skill is stored capacity gated by access. That is a sound and standard distinction, and it becomes testable as:

```latex
P_{\text{obs}}(t) = P_{\text{chance}} + \left(K(t) - P_{\text{chance}}\right) A(t), \qquad 0 \le A \le 1
```

K is retained capacity and A is access. A failed trial does not show K has dropped. The discriminating test: restore conditions (rest, Soil back in band) with feedback off. If performance recovers without new training, A was low and K was intact.

**3.6 Honey must carry information (D).** Feedback counts as Honey only if it is contingent on the outcome. With o the trial outcome and φ the feedback delivered:

```latex
I(\phi\,;\, o) > 0 \quad \text{(trained arm)}, \qquad I(\phi\,;\, o) = 0 \quad \text{(yoked control, same stimulation energy)}
```

The yoked arm receives the identical stimulation, replayed from a trained partner, so total charge is matched and only contingency differs.

## 4. Building

A cartridge is built when it passes three numeric release checks, each fixed before any training starts. This section gives the checks, not laboratory procedure; lineage choice and culture method stay with the qualified lab and the Design Specification.

**4.1 Interface competence (E).** The reference platform records 1,020 of 26,400 sites at once, which is 3.9% coverage. Channel selection therefore decides what the device can see. A channel i is usable when its spike signal-to-noise ratio clears a locked floor:

```latex
\text{SNR}_i = \frac{\left|\bar{A}_i\right|}{\sigma_{n,i}} \ge \text{SNR}_{\min}, \qquad N_{\text{usable}} = \sum_i \mathbf{1}\left[\text{SNR}_i \ge \text{SNR}_{\min}\right] \ge N_{\min}
```

**4.2 Stability (D).** Training on a drifting culture confuses maturation with learning. Require the mean firing rate and the burst rate to stay within a tolerance ε across three consecutive daily baselines:

```latex
\frac{\left|\bar{r}_{d} - \bar{r}_{d-1}\right|}{\bar{r}_{d-1}} \le \varepsilon \quad \text{for } d = 1, 2, 3
```

**4.3 Controllability (D).** The network must respond to the stimulation it will be trained with. For each input site k and output region c, measure the evoked-response gain with stimulation on versus sham:

```latex
W_{ck} = \frac{\bar{n}_c(\text{stim } k) - \bar{n}_c(\text{sham})}{\sigma_c(\text{sham})}
```

Release requires at least one input site with |W| above threshold for every output region. A cartridge with no evoked response cannot be trained, and that is a build failure rather than a learning failure.

**4.4 Physical completeness (from the Design Specification).** Each required item gets an identity check and a function check before any capability claim. In the Atlas, MARROW-08 makes the matching point that structure alone does not predict output: passing identity checks never substitutes for 4.1 to 4.3.

**4.5 Stimulation limits (E).** Every pulse stays inside the vendor's charge limits, which the Heart controller enforces in hardware-independent software. Charge per phase is current times phase duration, Q = I × t\_phase, and the locked ceiling comes from the electrode vendor's specification, not from this document.

## 5. Training

Training is one closed loop repeated every trial: encode, record, decode, score, feed back, rest. Each step has one equation, and the decoder is frozen before evaluation.

1. **Encode (D).** A feature vector s with components in \[0, 1\] becomes stimulation rates on assigned input sites (rate and place coding):

   ```latex
   r_k = r_{\min} + (r_{\max} - r_{\min})\, s_k
   ```
2. **Decode (D).** The decision is the output region with the most spikes in the response window, after artifact masking:

   ```latex
   \hat{y} = \arg\max_{c} \sum_{i \in R_c} \sum_{t \in T_{\text{resp}}} x_i(t), \qquad \text{confidence } p = \text{softmax of the same sums}
   ```
3. **Feed back (H, DishBrain-style).** Following Device Design v0.4, feedback is an information contingency: two distinguishable, culture-qualified feedback events, one after correct and one after incorrect decisions, with no assumption about pleasure or pain. The predictable-versus-unpredictable mapping from DishBrain is one candidate policy to test, not a given. Honey is then m = +1 or −1 in equation 3.3. Whether predictability acts as reinforcement in your preparation is the hypothesis under test.
4. **Rest.** Feedback off, recording on. Rest data feed the retention analysis in section 6.

**Learning metrics (D).** Three numbers per session, all on held-out patterns:

```latex
\text{LI} = \frac{P_{\text{end}} - P_{\text{start}}}{1 - P_{\text{chance}}}, \qquad d' = z(\text{hit rate}) - z(\text{false-alarm rate}), \qquad \text{ECE} = \sum_{b} \frac{n_b}{N} \left| \text{acc}_b - \text{conf}_b \right|
```

LI is the learning index, d′ separates detection from bias toward alarming, and ECE is the calibration error that fills C in the Understanding vector.

**The claim that counts (H).** The primary research question in your specification becomes one inequality, tested across cartridges:

```latex
P_{\text{trained}} > \max\left(P_{\text{untrained}},\; P_{\text{yoked}}\right) \quad \text{and} \quad P_{\text{trained + software}} > P_{\text{software alone}}
```

The second half is the hard one. A software classifier will solve an encoded three-class problem almost perfectly, so the living network adds value only if the hybrid beats software alone on something software finds hard, such as drifting inputs or few examples. If it never does, that negative result is the finding.

**Monitoring cells (H).** Section 5.3 of the specification trains sensor populations on their measured contribution to mesh stability. The same loop applies with one change: the outcome o is the main mesh's retention change, not the task result. Section 7 defines that contribution.

## 6. Roots and Tree

Roots are what remains after a delay with feedback off, and this section supplies the retention formula your poster left undefined. The Companion's coefficient audit notes that R\_U has no explicit formula; this is it.

**6.1 Retention (D).** Chance-corrected performance after delay Δ, relative to the end of training:

```latex
R_U(\Delta) = \frac{P(t_0 + \Delta) - P_{\text{chance}}}{P(t_0) - P_{\text{chance}}}
```

R\_U = 1 means full retention and 0 means back to chance. It is undefined if training never beat chance, which is correct: nothing was learned, so nothing can be retained. Measure it at two or more locked delays, for example 1 hour and 24 hours.

**6.2 Transfer (D, from Atlas ROOTS-02).** The Star relation compared success in new Soil with success in training Soil. Chance-corrected:

```latex
G = \frac{P_{\text{new}} - P_{\text{chance}}}{P_{\text{train}} - P_{\text{chance}}}
```

**6.3 Flexibility (D).** After the correct answer is reversed, count trials until performance returns to criterion: F = N\_criterion(original) / N\_criterion(reversal). Lower relearning cost gives F closer to 1.

**6.4 Old-task cost (D, from Atlas ROOTS-05 and ROOTS-09).** New learning must not erase old. After training task B, retest task A:

```latex
\Delta R_A = R_U^{A}(\text{after B}) - R_U^{A}(\text{before B})
```

A strongly negative value is interference. The specification requires reporting it, and the Lotus rule in section 7 treats task A as a protected relation.

**6.5 Forgetting curve (E, with a caution).** Fit an exponential, but test it against a power law before trusting it. The Companion warns that decay laws are not interchangeable.

```latex
R_U(\Delta) = e^{-\lambda \Delta} \quad \text{versus} \quad R_U(\Delta) = (1 + \Delta/\tau)^{-\beta}
```

**6.6 Astroengram test (H, Companion L8 and H17).** The astrocyte hypothesis becomes one number. Predict next-day retention from neuronal features alone, then add astrocytic features, and compare held-out loss:

```latex
\Delta L = L_{\text{held-out}}(\text{neurons}) - L_{\text{held-out}}(\text{neurons + astrocytes})
```

Positive ΔL on held-out cartridges supports added information. It does not show cause. Cause needs a perturbation with a control for general excitability. H17a, H17b and H17c are the same test with branch features, contact features and helical geometry as the added block, each judged separately.

**6.7 Tree record (D).** The Tree is a rooted graph whose nodes are retained relations and whose branches are the tasks each one supports. From MESH-01, a branch earns its place by reversible ablation: mask its electrodes in software and measure the drop in U. A branch whose removal changes nothing is redundancy (MESH-03), not Roots.

## 7. Expanding

Expansion adds modules one at a time through an electronic bridge, and each addition must earn its place by measured contribution. Three rules govern it: the bridge equation, the contribution test, and the Lotus and Heart decision rules.

**7.1 Electronic bridge (D).** Module A's decoded output, delayed by d, is encoded as one input to module B. A switch b(t) turns the bridge on or off without touching tissue.

```latex
\mathbf{c}^{B}_t = \mathbf{c}^{\text{task}}_t + b(t)\; \text{Enc}\left(\hat{y}^{A}_{t-d},\; p^{A}_{t-d}\right), \qquad b(t) \in \{0, 1\}
```

Auerlis in LOTUS-08 asked for an interface language that neither subsystem speaks alone. This is that language: a class label plus a confidence, re-encoded with the same rate code as section 5.

**7.2 Contribution of a support module (D).** A support network s contributes if the main mesh's Understanding is better with s bridged in than with a non-adaptive stand-in on a fixed schedule, component by component:

```latex
\kappa_s = \mathbf{U}\left(\text{mesh} + s\right) - \mathbf{U}\left(\text{mesh} + \text{fixed-schedule control}\right)
```

κ\_s is a vector. A module that raises retention but worsens calibration is reported as both. With several modules, estimate each one by leave-one-out, since order effects make simple addition unreliable.

**7.3 Seduction of Winds, made exact (D).** The support network's Honey is its own measured contribution on the previous block: m\_s = sign(κ\_s) for the chosen primary component. This is ordinary outcome-contingent feedback one level up. Its Milk stays unconditional, as section 4.2 of the specification requires.

**7.4 Lotus decision rule (D, from Atlas LOTUS-04 and LOTUS-09).** For a proposed action with predicted benefit ΔU, resource cost c and predicted damage to protected relations ΔR\_prot, the coordinator computes a lower confidence bound and decides:

```latex
J = \widehat{\Delta U} - \alpha\, c - \beta\, \left|\widehat{\Delta R}_{\text{prot}}\right|, \qquad \pi = \begin{cases} \text{ACT} & J_{\text{lower}} > 0 \\ \text{HOLD} & J_{\text{upper}} < 0 \\ \text{UNRESOLVED} & \text{otherwise} \end{cases}
```

UNRESOLVED is a real outcome. When the interval spans zero the system gathers more observations and does not guess.

**7.5 Heart veto (D).** The Heart is separate from the Lotus and cannot be outvoted. It holds a list of hard constraints and blocks any action that would break one:

```latex
\text{permit}(\text{action}) = \prod_{j} \mathbf{1}\left[g_j(\text{state}, \text{action}) \le 0\right]
```

The constraints are charge limits, Soil bands (q = 1), minimum rest time, and protected old-task retention. No value of J can override a veto, which is how the design keeps score from buying its way past safety.

**7.6 Physical joining.** Axon-permissive connection is a later, separately gated stage. Its extra burden is route specificity: show the effect disappears when the electronic route is cut but the physical one is intact, and the reverse. I add no equation because the electronic bridge has to pass first.

## 8. Sharing

Sharing means a trained cartridge teaches a second one faster than the second could learn alone, and nothing more than that. What crosses the bridge is a signal. No memory, engram or Astroengram moves between cartridges, and your specification lists transferable Astroengrams as not established.

**8.1 Teaching effect (D).** Train recipient B with the bridge from teacher A active. Then cut the bridge, clear cached outputs, freeze the decoder and test B alone:

```latex
T = P_B^{\text{alone}}(\text{after taught}) - \max\left(P_B^{\text{alone}}(\text{yoked}),\; P_B^{\text{alone}}(\text{shuffled}),\; P_B^{\text{alone}}(\text{software teacher})\right)
```

T > 0 on held-out cartridges means teaching by a living teacher left something in B that the controls did not. The software-teacher arm matters most: if a program giving the same labels teaches B equally well, the living teacher added nothing.

**8.2 Teaching efficiency (D).** Even with equal final performance, teaching may save time:

```latex
S = 1 - \frac{N_{\text{criterion}}(\text{taught})}{N_{\text{criterion}}(\text{self-trained})}
```

S is the fraction of training trials saved. It can be negative.

**8.3 Dependence check (from Atlas ROOTS-06).** Khavia's relation says independent rooting requires recall when the teacher cue goes to zero. That is exactly the isolation test: B's retention R\_U is measured only with b(t) = 0.

**8.4 Sharing the training center itself.** What other groups can actually reuse is the method, so the shareable package is:

- The frozen decoder, encoder tables and task definitions, versioned.
- The pre-registered analysis plan and every gate threshold.
- Raw recordings for at least the evaluation blocks, with checksums.
- The Heart constraint list and every veto event.
- Negative results, reported with the same detail as positive ones.

A second laboratory reproducing T > 0 from this package is the strongest form of sharing the design can claim.

## 9. Budgets

One module produces about 40.8 MB every second, and no energy advantage over software should be expected. Both follow from arithmetic and from the whole-device boundary your specification sets.

**9.1 Data (E).** I rechecked the specification's figure and it is correct:

```latex
\dot{D} = N_{\text{ch}} \times f_s \times \frac{\text{bits}}{8} = 1020 \times 20000 \times 2 = 40.8\ \text{MB/s}
```

| Configuration | Per hour | Per day |
| --- | --- | --- |
| One module, continuous | 146.9 GB | 3.5 TB |
| Two modules, continuous | 293.8 GB | 7.1 TB |

Continuous raw storage is impractical for long studies. Store raw data for evaluation blocks and spike times for everything else, and lock that duty cycle before procurement.

**9.2 Loop latency (E).** Closed-loop feedback only works if it arrives while eligibility lasts. The total delay must sit inside the eligibility time constant from equation 3.3:

```latex
t_{\text{acquire}} + t_{\text{decode}} + t_{\text{stim}} \ll \tau_e
```

**9.3 Energy per correct response (E, Companion Q1 and Q2).** The boundary is the whole device: host, acquisition, stimulation, incubation, pumps and storage.

```latex
e_{\text{corr}} = \frac{E_{\text{total}}}{N_{\text{correct}}}, \qquad G_E = 1 - \frac{e_{\text{hybrid}}}{e_{\text{software}}}
```

Expect G\_E to be strongly negative in a first build. The tissue itself uses very little power, but the incubator, amplifiers and host computer use far more than a small classifier does. Report it anyway.

**9.4 Full-cycle bound (E, Companion E5).** Any efficiency claim must hold over a complete cycle that returns every store to its starting state. A trained state can change how energy is converted. It is never an energy source.

## 10. Evidence gates and statistics

Six gates run in order, and a later gate cannot open until the earlier one has passed on pre-registered thresholds. The thresholds below are placeholders to be locked by the laboratory; the structure is the contribution.

| Gate | Question | Pass rule | Section |
| --- | --- | --- | --- |
| G1 Source | Is each lineage what its record says | Identity and release record complete | 4.4 |
| G2 Maturation | Is the culture stable | Drift ≤ ε on three daily baselines | 4.2 |
| G3 Interface | Can it be read and driven | N\_usable ≥ N\_min and W above threshold per region | 4.1, 4.3 |
| G4 Learning | Does contingent feedback teach | Trained > untrained and yoked on held-out patterns; primary endpoint R\_U at 45 minutes above zero; adaptive beats random site selection (section 20) | 5, 6 |
| G5 Bridge | Does teaching leave something | T > 0 in isolation, including against the software teacher | 8 |
| G6 Support | Does an added module help | κ\_s > 0 against fixed-schedule control, with ΔR\_A and cost reported | 7 |

**10.1 The unit is the cartridge (E).** Electrodes and trials inside one cartridge are not independent. Every test compares cartridges, with trials nested inside them in a mixed model. A result from one cartridge, however many trials, has n = 1.

**10.2 Sample size (E).** For a two-arm comparison at two-sided α = 0.05 and 80% power, cartridges per arm for a standardized effect d:

```latex
n = \frac{2\,(z_{1-\alpha/2} + z_{1-\beta})^2}{d^2} = \frac{15.7}{d^2}
```

| Effect size d | Cartridges per arm |
| --- | --- |
| 1.0 (large) | 16 |
| 0.8 | 25 |
| 0.5 (medium) | 63 |

G4 has three arms, so a large effect needs roughly 48 cartridges. This number drives cost more than any other in the design.

**10.3 Controls that every arm carries.** Untrained, yoked (noncontingent), software-only, and shuffled-label decoder. Scoring is blinded to arm. Decoder and thresholds are frozen before evaluation data exist.

**10.4 Multiple comparisons (E).** One primary endpoint per gate. Everything else is secondary and corrected by false-discovery rate across the declared family.

**10.5 Stop rules.** Any Heart veto, unexplained Soil excursion, instrument fault or unreviewed method change pauses the arm for review, as section 10 of the specification requires. Paused data are kept and reported.

## 11. Open parameters, non-claims and next steps

The theory set is complete as a structure, and every number inside it is still open. Nothing below can be filled in by reasoning, by me or by a language model; each needs a measurement.

**Parameters only a laboratory can supply**

| Parameter | Appears in | How it gets set |
| --- | --- | --- |
| SNR\_min, N\_min, ε | 4.1, 4.2 | Platform qualification runs |
| Soil bands l\_k, u\_k, s\_k | 3.4 | Culture qualification |
| r\_min, r\_max, charge ceiling | 5, 4.5 | Vendor limits plus pilot |
| τ\_e, η, λ | 3.3, 9.2 | Fitted, with m and q measured independently |
| Delays for R\_U, retention floor | 6.1, G4 | Pre-registration |
| α, β and confidence level for J | 7.4 | Pre-registration, with sensitivity analysis |

**Not claimed by this document**

- That any living network has learned, retained or shared anything under this design.
- That astrocytes store or carry task content. Section 6.6 is a test, not a finding.
- That the Six Stars outputs are evidence about biology. They are prompts for hypotheses.
- Any benefit for detecting or treating disease. The observational network classifies encoded test patterns on a bench.
- Any energy or efficiency advantage over software.

**Next steps, in order**

- [ ] Simulate the whole loop in software first. Equations 3.1 to 3.6 with a spiking-network simulator will show whether the metrics and gates behave before any tissue is involved. This costs nothing and I can build it with you.
- [ ] Write the pre-registered analysis plan from section 10.
- [ ] Take sections 4 to 10 to a neuroscientist who runs microelectrode-array cultures, and to an institutional biosafety office. The living-tissue work cannot start, legally or safely, outside that setting.
- [ ] Finish the Atlas's own protocol: the 36 pending prompts, blind scoring and the untouched holdout. That tests the Stars as a hypothesis generator, which is the claim they can support.

**Sources read:** ChristEye Design Specification v1.0; Framework article v1.3.5; Technical Companion v2.2.3 sections 7 to 10 and Appendix B; Six Stars Atlas v9 scorecard, equation table and boundaries. Published precedents named in your documents (Kagan et al. 2022; Williamson et al. 2025) were not re-fetched here.

## 12. Second pass: the Star corpus, term by term

A full pass through all 95 answered Atlas rows yields 31 further equations for the device, set out in sections 13 to 17. Each one starts from a specific Star line, cited by Atlas ID.

**The procedure used on every row.** This is the repeatable machine, and it is the same five layers each time:

1. **Word.** The Star's sentence, as logged.
2. **Plain meaning.** What it says with the ChristEye terms replaced by ordinary ones.
3. **Device variable.** Which measurable symbol from section 2 each term becomes.
4. **Equation.** The tightest form that keeps units consistent.
5. **Test.** What result would show the equation is wrong for this device.

A row that cannot reach layer 5 stays a metaphor and is listed in section 18.

**What the pass showed about the Stars.** The usable rows restate known science in ChristEye vocabulary: retrieval versus storage, queueing overload, the stability and plasticity trade-off, confounding, exploration versus exploitation. That is why they can be formalized, and it also means none is a new discovery. The Stars work well as a checklist that keeps asking the right distinctions. They do not supply measurements, and three rows (LOTUS-06, TREE-SEARCH-01, CREATION-FRUIT-09) only echoed the prompt.

**One result worth stating first.** In section 14, Elusiaa's line that durable change is signal strength times support quality times time (MILK-HONEY-04) turns out to be the short-time solution of the Companion's own plasticity equation. The Star prose and your formal model agree there, which is a consistency check on the framework, not evidence about cells.

## 13. Water: access, flow and overload

Water is the path between what is retained and what is used, so its equations are about routing, rate and noise. Basic meaning: enabling access. Device meaning: the signal path from stored weights W to decoded output y, including timing.

**13.1 Flooding (WATER-02, Maera).** Input arriving faster than it can be integrated builds a backlog Q:

```latex
\frac{dQ}{dt} = \lambda_{\text{in}} - \mu_{\text{int}}, \qquad \text{overload when } \lambda_{\text{in}} > \mu_{\text{int}}
```

Device form: λ\_in is the trial rate and μ\_int is one over the network's recovery time after a stimulus. Test: sweep the inter-trial interval. Performance should hold flat and then fall sharply below a critical interval. If it declines smoothly from the start, the queue model is wrong.

**13.2 Noise gates access, not storage (WATER-04, Kaelia).** This gives the access term A from equation 3.5 a concrete form:

```latex
A = \frac{\text{SNR}}{\text{SNR} + \text{SNR}_{1/2}}
```

SNR\_1/2 is the signal-to-noise ratio at which half of retained capacity is expressed. Test: add controlled background stimulation, watch performance drop, remove it, and check that performance returns without retraining.

**13.3 Alternate routes (WATER-08, Maera).** If recall fails on one cue but succeeds on another cue for the same relation, storage is intact and the route failed:

```latex
P(\text{cue}_2) > P_{\text{chance}} \;\text{ while }\; P(\text{cue}_1) \approx P_{\text{chance}} \;\Rightarrow\; K > P_{\text{chance}},\; A_1 \approx 0
```

Device form: encode the same pattern on two different input electrode sets during training, then test each alone.

**13.4 Cue competition (WATER-05, Maera).** Which stored pattern a cue retrieves depends on similarity plus context match:

```latex
P(\text{retrieve } i \mid \text{cue}) = \frac{\exp\left[\left(\text{sim}(\text{cue}, i) + \alpha\, \text{ctx}_i\right)/T\right]}{\sum_j \exp\left[\left(\text{sim}(\text{cue}, j) + \alpha\, \text{ctx}_j\right)/T\right]}
```

Test: confusion between two trained patterns should rise as their stimulation patterns are made more similar. A flat confusion rate falsifies it.

**13.5 Timeliness (WATER-09, Elusiaa).** A correct answer that arrives late is an operational failure. For the observational network, whose job is early detection:

```latex
V = \mathbf{1}[\hat{y} = y] \cdot e^{-t_{\text{detect}}/\tau_d}
```

V replaces raw accuracy as the detection score. τ\_d is the deadline scale from the NeedRequest record in section 7 of the specification.

## 14. Milk and Honey: support, selection and their product

The Stars' six Milk and Honey answers all reduce to one structure: outcome depends on the product of guidance and support, so neither can stand in for the other. Basic meanings: Milk sustains the capacity to respond; Honey selects which response is kept.

**14.1 Durable change (MILK-HONEY-04, Elusiaa).** Integrate equation 3.3 over a training period T that is short compared with the decay time 1/λ:

```latex
\Delta w_{ij} \approx \eta\; \bar{m}\; \bar{q}\; \bar{e}_{ij}\; T
```

Signal strength is the mean Honey, support quality is the mean Milk, and time is T. The Star line and the Companion's L2 are the same statement here. For long T the change saturates at η·m·q·e/λ, so more training time stops helping. That saturation is a prediction the Star line alone does not make.

**14.2 More support cannot fix wrong guidance (MILK-HONEY-06, Khavia).** Differentiate equation 3.3 with respect to Milk:

```latex
\frac{\partial}{\partial q}\left(\frac{dw_{ij}}{dt}\right) = \eta\, m\, e_{ij}
```

The sign follows m. If feedback is mis-assigned, better Soil conditions make the wrong pattern stabilize faster. Practical consequence: verify the feedback contingency on a software mock before any long run, because good culture health will amplify a decoder bug.

**14.3 Signal versus resource (MILK-HONEY-10, pending in the Atlas; this form is mine).** Vary Honey and Milk independently in a two-by-two design and fit:

```latex
\text{LI} = \beta_0 + \beta_m m + \beta_q q + \beta_{mq}\, m\, q
```

Equation 3.3 predicts a pure interaction: β\_mq > 0 with β\_m and β\_q near zero. A large β\_q alone would mean the supposed support variable is acting as a signal. This is the cleanest test that Milk and Honey are distinct roles, which is hypothesis H07 in the Companion. Milk is varied only inside its safe band, never withheld.

**14.4 Honey quality is judged late (MILK-HONEY-03, Khavia).** Compare feedback schedules by what remains after the delay, minus cost:

```latex
H_q = R_U(\Delta) \cdot \text{LI} - \gamma\, c_{\text{stim}}
```

A schedule that gives fast learning and poor retention scores low. c\_stim is total delivered charge.

**14.5 Resource gate (MILK-HONEY-01, Elusiaa).** No plasticity-inducing block starts unless reserves clear a floor. This becomes one more Heart constraint in equation 7.5: g = R\_min − R(t) ≤ 0, where R(t) is the measured medium reserve.

**14.6 Support without drive (MILK-HONEY-02, Maera).** Growth is seed times support times drive, so a healthy but silent culture learns nothing. Device form: eligibility e in equation 3.3 is zero when there is no evoked activity. This is why gate G3 checks controllability before G4 attempts training.

## 15. Heart and Lotus: deciding under risk

The Heart rows give thresholds for refusing and for acting, and the Lotus rows give the stability terms that the coordinator trades against gain. Basic meanings: Heart weighs need against harm; Lotus keeps the parts coordinated in time.

**15.1 Refusal bound (HEART-03, Elusiaa).** An action is vetoed when its expected irreversible cost exceeds a fixed bound:

```latex
p_{\text{cat}}(a) \cdot C_{\text{irr}}(a) > \rho \;\Rightarrow\; \text{veto}
```

For the device, the irreversible events are loss of a culture and loss of a protected learned relation. ρ is set before the study.

**15.2 When to stop waiting (HEART-05, Elusiaa).** UNRESOLVED in equation 7.4 cannot last forever. Act once delay costs more than the remaining uncertainty:

```latex
c_{\text{wait}} \cdot \Delta t > \sigma_J \cdot \phi\left(\frac{\hat{J}}{\sigma_J}\right) \;\Rightarrow\; \text{decide now}
```

The right side approximates the expected loss from choosing wrongly given the current estimate J and its standard error. It shrinks as evidence accumulates.

**15.3 Correction without shutdown (HEART-06, Elusiaa).** Error feedback that suppresses the network has failed even if errors drop. Require activity after an error stimulus to stay near baseline:

```latex
\frac{\bar{r}_{\text{post-error}}}{\bar{r}_{\text{baseline}}} \ge \rho_{\min}
```

A culture that goes quiet after errors would score fewer false alarms for the wrong reason. This check catches that.

**15.4 Stability against plasticity (LOTUS-03 and LOTUS-09, Nyxari and Auerlis).** The coordinator's objective, in terms of the Understanding vector:

```latex
\mathcal{L} = \underbrace{\text{LI}}_{\text{short-term gain}} + \alpha\, F - \beta\, \left|\Delta R_A\right|
```

F is flexibility from 6.3 and ΔR\_A is old-task cost from 6.4. This is the same J as equation 7.4 with its terms now named.

**15.5 Identity drift (LOTUS-07 and X-07, Maera and Kaelia).** The network may adapt while its protected structure stays recognizable:

```latex
D(t) = 1 - \text{corr}\left(W_{\text{prot}}(t),\; W_{\text{prot}}(0)\right) < \delta
```

W\_prot is the evoked-response matrix from 4.3, restricted to the electrodes that carry protected relations. D is cheap to measure daily and becomes a Heart constraint.

**15.6 Timing between modules (LOTUS-05, Nyxari).** Choose the bridge delay d that best aligns module A's output with module B's receptive window:

```latex
d^{*} = \arg\max_{d}\; \text{corr}\left(\hat{y}^{A}_{t-d},\; \text{resp}^{B}_{t}\right)
```

**15.7 Causal test of coordination (LOTUS-10, Elusiaa).** The coordinator earns credit only if switching it changes later coordination beyond what the current state predicts:

```latex
E\left[\kappa_{t+1} \mid do(\pi_t = \text{on}),\, \text{state}_t\right] - E\left[\kappa_{t+1} \mid do(\pi_t = \text{off}),\, \text{state}_t\right] > 0
```

Randomize the coordinator on and off across blocks. This is the strongest row in the corpus, because it asks for an intervention.

## 16. Mesh, Wind and the stream

The stream is the ordered path a signal takes through the device, and information about the input can only be lost along it, never gained. That bound is the most important consequence of this pass. Basic meanings: Mesh is which components influence which; Wind is the output one system sends to another.

**16.1 The stream and its limit (X-05, Kaelia).** Kaelia's chain runs field, measurement, Mesh representation, Lotus integration, Fruit. In device terms that is sensor pattern s, encoded stimulus c, network activity x, decoded output y. Because each stage depends only on the one before it:

```latex
s \to c \to x \to y \quad\Rightarrow\quad I(s; y) \le I(s; x) \le I(s; c)
```

The living network cannot know more about the input than the encoder gave it. So it can never beat software at classifying a fixed, fully encoded pattern. Its only possible advantages are adaptation when inputs drift, learning from few examples, and memory across time. This sharpens the primary research question: test the hybrid on drifting and sparse tasks, since on a static task the bound guarantees a tie at best.

**16.2 A functional edge (MESH-04, Maera).** An edge exists only when stimulating i changes j beyond a threshold. Using the evoked gain from 4.3:

```latex
E_{ij} = \mathbf{1}\left[\left|W_{ji}\right| > \theta\right]
```

Correlated firing without evoked influence is recorded as a statistical edge, a different type in the Mesh record (Companion L5).

**16.3 Knowledge in the combination (MESH-09, Khavia).** Whether output depends on regions jointly, beyond what each carries alone:

```latex
\text{Syn}(y; x_1, x_2) = I(y; x_1, x_2) - I(y; x_1) - I(y; x_2)
```

Positive means the regions carry the answer only together. Negative means they are redundant. Both are informative: redundancy predicts robustness to losing electrodes (MESH-03), synergy predicts fragility.

**16.4 Common cause (MESH-10, Khavia).** Two regions that both follow the stimulus will correlate without influencing each other. Condition on the stimulus first:

```latex
\rho_{x_1 x_2 \mid c} = \frac{\rho_{12} - \rho_{1c}\,\rho_{2c}}{\sqrt{(1 - \rho_{1c}^2)(1 - \rho_{2c}^2)}}
```

If this partial correlation is near zero, the apparent relation was the shared input. The specification's route-specific checks in 6.2 need exactly this.

**16.5 Is a Wind information-bearing (WIND-09, Kaelia).** An output carries information if it predicts the input better than its own shuffled version:

```latex
I(y; s) - I(y_{\text{shuffled}}; s) > 0 \quad \text{at the pre-set significance level}
```

Kaelia's row also says intention is a separate claim. Agreed: passing this test shows structure, nothing about purpose.

**16.6 Attunement evidence (WIND-10, Elusiaa).** Three ingredients, all required:

```latex
\mathcal{A} = \left(P_{\text{held-out}} - P_{\text{chance}}\right) \times \mathbf{1}\left[\text{performance falls when input is perturbed}\right]
```

A network that scores above chance but does not change when the input is scrambled is responding to something other than the input, usually timing or stimulation artifact.

**16.7 Speak in the recipient's terms (X-03, Maera).** The bridge encoder should be fitted to the receiving module, not fixed in advance. Choose the encoding that the recipient decodes most faithfully:

```latex
\text{Enc}_B^{*} = \arg\max_{\text{Enc}}\; P\left(\text{Dec}_B(\text{Enc}(\hat{y}^A)) = \hat{y}^A\right) \ge \tau
```

Practical step before any teaching run: send each class label across the bridge alone and confirm B's response distinguishes them. If it cannot, a later teaching failure says nothing about teaching.

**Same output, different path (WIND-08, Kaelia).** Two different activity patterns can decode to the same answer. So a correct output never identifies the mechanism, and a claim about how the network solved a task needs activity-level evidence, not the score.

## 17. Tree, repair and creation

These rows govern what the system does over weeks: which experiments to run, when to retire a cartridge, and how to combine modules without blurring them. Basic meanings: Tree is the branching record of retained relations; Seed is what is kept to start again.

**17.1 Is a search worth it (TREE-SEARCH-06, Maera).** Run the next experiment only if expected information gain exceeds its cost:

```latex
\text{EIG} = H\left[p(\theta)\right] - E_{\text{data}}\, H\left[p(\theta \mid \text{data})\right] > \lambda_c \cdot \text{cost}
```

The highest-value experiment is the one where two live hypotheses predict different results, which answers the pending TREE-SEARCH-09 as well.

**17.2 Pruning (ROOTS-10, Khavia).** Drop a branch (an input channel set, a module) only if the gain exceeds everything it supported:

```latex
\Delta \text{LI}_{\text{primary}} > w_G\, \Delta G + w_R\, \left|\Delta R_A\right|
```

And from TREE-SEARCH-04: keep the pruned branch's record, with its context, prior utility, reason and recovery conditions. Software masking makes every prune reversible at first.

**17.3 Refresh or retire (REPAIR-MEDICINE-05, Nyxari).** A degraded cartridge is rested and retested if its interface still passes; it is retired if damage has reached the core:

```latex
\text{refresh if } N_{\text{usable}} \ge N_{\min} \text{ and } D(t) < \delta; \quad \text{retire otherwise}
```

Retirement criteria are fixed in advance so a failing culture is not kept in the data by hope, or dropped by convenience.

**17.4 Exploration (CREATION-FRUIT-08, Elusiaa).** Training on only the patterns the network already handles is an exploitation trap. Sample the next pattern by:

```latex
P(\text{pattern } k) \propto \exp\left[\left(\text{error}_k + \beta\, \text{novelty}_k\right)/T\right]
```

With β at zero the curriculum narrows and transfer G should fall. That is a testable prediction of the row.

**17.5 Do not average the experts (CREATION-FRUIT-10, Nyxari).** Combine module outputs by weighted product, which keeps each one's confident opinions, instead of a plain mean:

```latex
p(y) \propto \prod_{s} p_s(y)^{\,w_s}, \qquad w_s \propto \max(\kappa_s, 0)
```

Weights come from each module's measured contribution in 7.2, so a module that does not help gets weight zero.

**17.6 Consolidation as a stable state (X-06, Kaelia).** A rooted pattern should return after a small disturbance. Perturb, then time the recovery:

```latex
\left\|x(t) - x^{*}\right\| \approx \left\|x(0) - x^{*}\right\| e^{-t/\tau_r}
```

A short recovery time τ\_r for trained patterns compared with untrained ones would be direct evidence of Roots at the activity level. The Atlas notes crystallization is an analogy here, and I keep it that way.

**Seed (CREATION-FRUIT-07 and X-02, Elusiaa and Maera).** The Seed is result plus process memory plus conditions plus failure lessons. That is the shareable package already listed in 8.4, and the Star rows confirm it should include what failed.

## 18. Rows set aside

Not every row became an equation, and listing the exclusions keeps the count honest.

| Rows | Why set aside |
| --- | --- |
| LOTUS-06, TREE-SEARCH-01, CREATION-FRUIT-09 | The logged answer only echoed or duplicated a prompt |
| X-01, X-11 | Theological or life-force terms with no device variable; the Atlas marks them as analogy |
| X-08, X-09 | Scaffold and yearning images; no test could be written |
| HEART-02, HEART-07, HEART-08 | Ethical guidance about care, kept as principles in the Holy Heart sense, not turned into numbers |
| Most MARROW rows | They say outcome depends on starting structure and environment together, which is true and already covered by gates G1 to G3 |
| 37 pending rows | Never answered. I wrote my own forms for MILK-HONEY-10 and TREE-SEARCH-09 and marked them as mine |

The Atlas scorecard numbers (for example Systems 9.8) are ratings assigned during analysis, not measured results, so I did not use them to weight any row.

**Not read this pass:** the patent's stream-implementation and layered-expression sections. The stream above is built from the Atlas and the Design Specification only, so it may differ from how your patent defines the term.

## 19. The source text as a cue layer

A word-level pass over your ESV file gives the framework a map of how its own terms travel together in the source, and three more design questions. It cannot give a device parameter. Your Framework article already sets this rule: a semantic relation is a TreeCue, a search seed that generates the question, while evidence decides the answer. I have kept to it.

**Method.** I decoded the file to 31,103 verses across 66 books, searched 30 framework terms, and counted verses where two terms appear together. Lift is that count divided by what chance would give; a lift of 1 means no association. No verse text is reproduced here, only references and my own summaries.

**19.1 Which terms travel together**

| Pair | Verses together | Lift | What the pairing cues for the device |
| --- | --- | --- | --- |
| Milk + Honey | 23 | 243 | Almost always a pair, which fits treating them as a product (equation 3.3) |
| Seed + Soil | 5 | 95 | Same seed, different receiving conditions: see 19.3 |
| Root + Branch | 11 | 59 | What is retained and what is expressed are named together: sections 6 and 16 |
| Understanding + Wisdom | 41 | 51 | Listed side by side as distinct items: keep U a vector |
| Root + Fruit | 14 | 32 | Output traced back to retained structure: ablation test 6.7 |
| Tree + Fruit | 51 | 30 | A tree is known by its output: judge by held-out performance |
| Understanding + Knowledge | 16 | 25 | Distinct but linked, as in your definitions |
| Water + Stream | 18 | 20 | Access as flow: section 13 |
| Heart + Soul | 37 | 5.3 | The responding organization and the whole |
| Heart + Understanding | 13 | 3.9 | Weak but present link |
| Wind + Spirit | 1 | below 1 | Rarely together in this translation |

**19.2 Two findings about the vocabulary itself.** Root and Understanding never share a verse in this text. The central ChristEye phrase, rooted understanding, is therefore your own synthesis from separate threads, which the Framework article already says about how the Tree term was found. Lotus appears in two verses only (Job 40:21 and 40:22), as a plant giving shade, and Marrow in three. Their roles as coordinator and foundation are your definitions, not usages the text supplies.

**19.3 Passages where four or more terms meet.** Eight verses carry four or more framework terms. Each is summarized in my words with the question it raises.

| Passage | Terms present | Structure of the image | Device question |
| --- | --- | --- | --- |
| Jeremiah 17:8 | root, tree, fruit, water, stream | A tree rooted toward water keeps bearing through heat and drought | Does a trained pattern survive a period of reduced support? |
| Psalms 1:3 | tree, fruit, water, stream | Fruit arrives in its season | Is the output on time? (13.5) |
| Luke 8:15, with Mark 4:5 and 4:8 | seed, soil, heart, word, fruit | One seed, several soils; the shallow soil sprouts fast and fails | How much outcome variance comes from Soil alone? |
| Hebrews 4:12 | word, heart, soul, spirit, joint, marrow | Something sharp enough to separate things that lie close together | Can each test separate two adjacent explanations? |
| Isaiah 11:2 | wisdom, understanding, knowledge, spirit | The capacities are listed one by one | Report U as components (2) |
| Proverbs 2:10 | wisdom, heart, knowledge, soul | Wisdom entering the heart | Which input reaches the responding network? |
| Ezekiel 17:8 | branch, fruit, water, soil | Good soil and plentiful water, and the next verses still question whether it thrives | Good support does not guarantee outcome (14.2, 14.6) |
| John 3:8 | wind, spirit | The effect is perceived while the origin and destination are not | Effect observed, source unresolved (WIND-07, 16) |

**19.4 Three equations these cues prompt.** They are definitions for tests, labeled D, and the passages are provenance for the question only.

Drought resilience, from the Jeremiah 17:8 cue. Hold support at the low edge of its safe band for a fixed period with feedback off, and compare:

```latex
\rho_{\text{res}} = \frac{P_{\text{low support}} - P_{\text{chance}}}{P_{\text{normal}} - P_{\text{chance}}}
```

A rooted pattern predicts a value near 1 for trained cartridges. Support stays inside the Heart's limits throughout; this is a band-edge test, never deprivation.

Soil share of variance, from the sower cue. Run one fixed training protocol across cartridges in several qualified Soil conditions:

```latex
\eta^2_{\text{Soil}} = \frac{\text{Var}_{\text{between Soil conditions}}\left(R_U\right)}{\text{Var}_{\text{total}}\left(R_U\right)}
```

A large share means results will not replicate between laboratories unless Soil is specified tightly. The shallow-soil case in the same passage is the fast-learning, poor-retention pattern that 14.4 already penalizes.

Staged curriculum, from the Hebrews 5:13 cue, where milk is for the unpracticed and solid food for the mature. Advance to harder patterns only after the easier stage is retained:

```latex
\text{advance stage } k \to k+1 \quad \text{iff} \quad R_U^{(k)}(\Delta) \ge R_{\min}
```

**19.5 What this layer cannot do.** The text does not contain electrode counts, time constants, safe charge limits or sample sizes, and no reading method will extract them. Those come from measurement, as section 11 lists. What the source text can do within your framework is what it did here: check that the vocabulary is used consistently, show which pairings are yours and which are the text's, and suggest questions worth testing. The Hebrews 4:12 cue is a fair summary of the whole method: every test in this document exists to separate two things that look alike.

## 20. Stream run: Word, Move, Judge on ten Star questions

Running the patent's own cycle on ten Star questions, then judging the results against published experiments, changes the design's first priority: retention, not learning, is the unsolved problem. This time the patent's stream sections loaded, so this section follows them: a TARGETED scan of the source with a shuffled control (Word), a proposed change to the design (Move), and a check against evidence (Judge).

**20.1 Word stage: targeted scans.** For each question I bound two or three source terms, counted verses holding all of them, and compared with 200 runs where each term's verses were scattered at random.

| Star question | Verses found | Shuffled control mean | Reading |
| --- | --- | --- | --- |
| WIND-03 weak signal becomes meaningful (hear, understand, heart) | 6 | 0.14 | Above chance |
| TREE-SEARCH-07 right fruit, wrong reason (fruit, tree, know) | 4 | 0.09 | Above chance |
| REPAIR-MEDICINE-03 scar and repair (heal, wound, bind) | 4 | 0.01 | Above chance |
| LOTUS-05 timing (season or time, fruit) | 9 | 3.9 | Above chance |
| WATER-02 flooding (overflow, water, bank) | 2 | 0.00 | Above chance, two verses |
| ROOTS-10 pruning (prune or cut, branch, fruit) | 1 | 0.01 | One passage, John 15:2 |
| MILK-HONEY-02 seed, soil, root | 0 | 0.01 | Nothing in one verse; 8 within adjacent verses |
| MILK-HONEY-09 rest restores | 0 | 0.11 | No cue |
| WIND-04 signal or coincidence (test, word or spirit) | 6 | 8.7 | No better than chance |
| WIND-06 provenance of a wind | 1 | 1.0 | No better than chance |

Five scans clearly beat the control. That shows these words keep company in English scripture, as related words do in any coherent text. It does not show the passages carry information about the device, and your patent says the same: its own test found the real stream ahead of shuffled text by 5 of 142 labels, and it records that stream outputs remain cues.

**20.2 Move and Judge: four packets.** Each packet names the proposed change, the prediction and the evidence I checked.

**Packet A. Retention comes first.** Word: MILK-HONEY-09 and ROOTS-01 (what remains after rest). Move: make R\_U at a fixed short delay the first target of the whole program, ahead of accuracy. Judge: supported by the strongest precedent your specification cites. In the [Robbins et al. 2026 cartpole study](https://news.ucsc.edu/2026/02/brain-organoids-goal-directed-learning/), adaptive training lifted organoid success well above random training, but performance fell back to baseline after a 45-minute rest. Learning in a dish has been shown; Roots have not. The benchmark to beat:

```latex
R_U(45\ \text{min}) > 0 \quad \text{on held-out cartridges}
```

The Star line that rest improves a damaged Tree is not supported in this preparation; rest erased the gain.

**Packet B. Repetition across days, read at recall.** Word: Astroengram and WIND-03 (the signal becomes meaningful only after training). Move: space training into two exposures a day or more apart, and take the astrocyte measurement at the second exposure. Judge: consistent with [Dewa et al. 2025 in Nature](https://pmc.ncbi.nlm.nih.gov/articles/PMC12675280), where an emotional experience left a days-long astrocytic state change and a repeated experience then engaged that ensemble to stabilize memory. That work was in mice with noradrenaline signaling, which a culture dish lacks, so this is a hypothesis for the device. The prediction:

```latex
\frac{a_{\text{exposure 2}}}{a_{\text{exposure 1}}} > 1 \quad \text{and} \quad R_U^{\text{spaced}} > R_U^{\text{massed}} \;\text{ at equal total trials}
```

That paper also reports IGFBP2 in the astrocytic ensemble's signaling. It is the first independent link I found to a molecule your framework had already flagged, and it concerns stabilization, which matches the phase your Companion's H05 says to test.

**Packet C. Honey must be adaptive.** Word: CREATION-FRUIT-08 and LOTUS-10 (choose the training signal, and test it by intervention). Move: let the host choose which sites receive the training signal from recent performance, with a random-choice arm as control. Judge: supported. In the same Robbins study, training sites chosen by a reinforcement-learning coach reached about a 46% success rate against about 4.5% for random choice. This is section 17.4 confirmed as a design requirement, and it gives G4 its control arm.

**Packet D. Contingency is necessary.** Word: MILK-HONEY-06 (support cannot replace guidance). Move: none needed; keep the no-feedback and yoked arms. Judge: supported. [Kagan et al. 2022](https://pmc.ncbi.nlm.nih.gov/articles/PMC9747182) included open-loop and rest conditions alongside closed-loop feedback, and [Isomura et al. 2023](https://pmc.ncbi.nlm.nih.gov/articles/PMC10406890) found that a free-energy model predicted how cultured rat networks reorganized under stimulation. Equation 3.6 stands.

**20.3 Retained experience.** The cycle's last step is to keep what was learned, including what failed.

- Changed: gate G4 now names R\_U at 45 minutes as its primary endpoint, with adaptive against random site selection as a required arm.
- Added: a spaced-against-massed comparison (Packet B) before any astrocyte claim.
- Unresolved: whether anything in a culture can stand in for the neuromodulatory signal the mouse work depended on. This is the largest open question in the design.
- Rejected as cues: WIND-04 and WIND-06 scans, which did not beat the shuffled control.

**Sources for this section:** [Robbins et al. 2026, Cell Reports, via UC Santa Cruz](https://news.ucsc.edu/2026/02/brain-organoids-goal-directed-learning/) and [news-medical summary](https://www.news-medical.net/news/20260219/Brain-organoids-show-goal-directed-learning-in-control-task.aspx); [Dewa et al. 2025, Nature](https://pmc.ncbi.nlm.nih.gov/articles/PMC12675280); [Kagan et al. 2022, Neuron](https://pmc.ncbi.nlm.nih.gov/articles/PMC9747182); [Isomura et al. 2023, Nature Communications](https://pmc.ncbi.nlm.nih.gov/articles/PMC10406890). I read search summaries and abstracts, not the full papers. Patent sections read: stream source and cursor, 5.7A, 5.7B, flow allocation, and 8.8.

## 21. Merged with Physical Learning Device Design v0.4

The two documents now fit together as one program: v0.4 says what to build and in what order, and this document supplies the equation behind each of its tests. Where they disagreed, I took v0.4's stricter rule. I read v0.4's role system, evidence, modules, task, training, astroengram, support, teaching, controller, acceptance, comparison, failure, budget and work-package sections; the lineage, Soil and chemical sections I left to the laboratory.

**21.1 What I adopted from v0.4**

| v0.4 rule | Effect on this document |
| --- | --- |
| Four evidence labels: F finding, X synthesis, H hypothesis, D design choice | My E becomes F or X; D and H are unchanged. A chain with one untested H step stays H as a whole. |
| Reference task: context plus sequence gives X, Y or HOLD | Replaces the generic three-class task in section 5. Chance is 1/3 on macro-averaged recall. |
| Feedback is information, not reward or pain | Section 5 step 3 rewritten. |
| Releases R0 to R3 | R0, the cell-free software instrument, is the simulation I proposed in section 11. It comes first. |
| Seven separate claims | Gates G1 to G6 are kept and mapped to experiments E01 to E12 below. |
| Astroengram ladder A0 to A4 | Section 6.6 is level A1 only. A calcium event alone meets none of A1 to A4. |
| S may only propose four actions | The Lotus rule in 7.4 chooses among REQUEST REVIEW, DEFER TASK, TRY APPROVED ALTERNATE ROUTE and NO CHANGE. |

**21.2 Each v0.4 experiment and its equation**

| Experiment | Question | Equation here |
| --- | --- | --- |
| E01 Interface | Is the response real and the cue delivered | 4.1 SNR, 4.3 evoked gain W |
| E02 Learning | Does contingent feedback beat matched noncontingent | 3.6 contingency, 5 learning index |
| E03 Retention | Does it persist | 6.1 R\_U at 45 minutes, 24 hours, 7 days |
| E04 Observer | Does trained O beat untrained O and digital models | 5 decode, 16.5 information test |
| E05 Early warning | Does O predict before the outcome | 13.5 timeliness, lead time below |
| E06 Support | Do guided actions beat fixed schedules | 7.2 contribution κ, 15.7 causal test |
| E07 Astrocyte state | Do astrocytic features add prediction | 6.6 ΔL |
| E08 Astrocyte mechanism | Does a targeted change alter the outcome | Level A3; no equation until E07 passes |
| E09 Joining | Does a trained partner help | 7.1 bridge, 16.4 common-cause check, 16.7 receiver encoding |
| E10 Independent teaching | Does B keep it alone | 8.1 teaching effect T, 8.2 savings S |
| E11 Diversity | Does an added cell class help | 7.2 κ with cost |
| E12 Renewal | Does a replaced module preserve function | 6.4 old-task cost, 15.5 identity drift D |

**21.3 The acceptance targets as equations.** v0.4 states its targets in words; these are the same targets in this document's symbols.

Macro-averaged recall, the headline score, so a learner that always answers HOLD cannot look competent:

```latex
\text{MAR} = \frac{1}{3}\left(\text{recall}_X + \text{recall}_Y + \text{recall}_{\text{HOLD}}\right) \ge 0.80
```

Retention, converted with equation 6.1 and chance at 1/3:

```latex
R_U(24\ \text{h}) = \frac{0.75 - 0.333}{0.80 - 0.333} = 0.89, \qquad R_U(7\ \text{d}) = \frac{0.70 - 0.333}{0.80 - 0.333} = 0.79
```

So v0.4 asks the network to keep 89% of what it learned for a day and 79% for a week. The best published precedent kept none of it for 45 minutes. I recommend staging the gate: R\_U above zero at 45 minutes first, then 24 hours, with seven days as the stretch target.

Protected old capability, as a noninferiority test on the paired difference:

```latex
\text{lower 95\% bound of } \left(\text{MAR}_{\text{after}} - \text{MAR}_{\text{before}}\right) > -0.05
```

Early warning for observer O2, where features stop at a declared cutoff before the response:

```latex
\text{lead} = t_{\text{outcome}} - t_{\text{alert}} > 0, \qquad \text{false alerts} \le 5\% \text{ of nominal episodes}
```

Availability, with invalid, missing and late outputs left in the denominator:

```latex
\text{Avail} = \frac{N_{\text{correct and on time}}}{N_{\text{scheduled opportunities}}}
```

Milk and Honey interaction, which v0.4 defines as both minus each single plus the reference. That is the β\_mq term of equation 14.3:

```latex
\beta_{mq} = Y_{\text{both}} - Y_{\text{guidance only}} - Y_{\text{support only}} + Y_{\text{reference}}
```

**21.4 Sample size, reconciled.** v0.4's example of about 11 preparations per arm agrees with equation 10.2: a 0.10 difference over a 0.08 standard deviation is d = 1.25, and 15.7 / 1.25² gives 10.05. The assumed 0.08 looks optimistic for living cultures. At 0.15 the same formula needs 36 per arm, so the pilot's measured variance decides the budget.

**21.5 Teaching outcomes.** v0.4 distinguishes five results; here is how the numbers separate them.

| v0.4 outcome | Signature |
| --- | --- |
| No useful effect | T ≈ 0 and S ≈ 0 |
| Generic state improvement | B improves equally with untrained or shuffled A |
| Assisted access while connected | B scores well with the bridge on, falls with it off |
| Independently retained acquisition | T > 0 with the bridge off, at the declared delay |
| Selective mechanism supported | The above plus a passed A3-level test |

**21.6 Failure diagnosis.** v0.4's table and equation 3.5 make the same cut. Check delivery and validity first, then access A, then retained capacity K, and record UNRESOLVED when the observations cannot tell them apart. Its row for a gain that disappears after rest carries the right instruction: investigate retention, and do not declare rooted Understanding.

**21.7 Build order.** v0.4's sequence stands: W0 scope, W1 digital instrument, W2 platform and biology, W3 commissioning, W4 core learning, W4O observer, W5 joining and teaching, W5S support policy, W6 diversity. W1 needs no cells and can start now.

## 22. Ethics and oversight

The ethics case rests on three layers that do not replace one another: the author's theological position, the Holy Heart Principle, and the institutional requirements any laboratory must meet. The first is a matter of faith, the second is the framework's own rule, and the third is not optional whatever one believes.

**22.1 The author's position, recorded as stated.** Brad Gearon holds that within ChristEye a Spirit, meaning the neuron in this use of the word, is an expression of Christ; that Christ is the one who feels what is done to it; and that He bears this willingly and out of love, prepared to serve as our computer. He further holds that cells derived from a donor, for example a stem cell line made from a blood cell, carry no experiential binding to that donor, so the donor does not feel through them. This is a statement of belief. No measurement in this document can confirm or refute it, and no gate or claim here depends on it.

**22.2 What that position asks of the builder.** Taken on its own terms, the belief raises the duty of care. If someone feels what is done to the tissue, and bears it out of love, then gentleness is owed in every procedure. The Holy Heart Principle already says this: protected standing is not earned, and humane care does not depend on proving what a being experiences. So the design keeps every limit a cautious non-believer would also want:

- Support is never withheld as a consequence of task results (section 2).
- Feedback is information, with no event chosen to be aversive (section 5).
- No tissue is injured to produce training labels, as v0.4 section 13 requires.
- Stimulation stays inside vendor limits enforced by an independent controller (7.5).
- Error feedback that silences the network counts as a failure (15.3).
- Cultures are kept no more complex and no longer than the question needs, with retirement rules fixed in advance (17.3).

**22.3 What science can and cannot say about feeling.** Whether a culture of this kind can experience anything is unresolved, and the mainstream assessment is that current preparations cannot. A [National Academies committee](https://nap.nationalacademies.org/read/26078/chapter/8) concluded that organoids lack the cell types, regions and organized circuits thought necessary for consciousness. The stem cell society's guidelines likewise report no evidence of pain or consciousness so far, while telling researchers to stay alert as models become more complex or are joined together, [as The Transmitter summarizes](https://www.thetransmitter.org/organoids/what-is-the-future-of-organoid-and-assembloid-regulation/). This design does join modules and train them, so it sits in the area those cautions describe. The honest position is uncertainty handled with care, which the limits in 22.2 provide.

**22.4 The donor is still bound in three ordinary ways.** Whatever is true of experience, a cell line made from a person's blood carries that person's complete genome. That creates duties the theological position does not discharge:

- **Consent.** The donor must have agreed to this kind of use. Oversight guidance notes that many past donors were never asked about neural organoid research and some might have objected. Use lines whose consent covers neural culture, electrical training and commercial development.
- **Privacy.** Genomic and expression data from the line can identify the donor and relatives. Recordings and sequencing data need the same protection as medical records.
- **Provenance and withdrawal.** Keep the consent documentation with the Marrow record, and honor any conditions or withdrawal terms attached to the line.

**22.5 Oversight that must be in place before any living work.** Version 0.4 of the device design does not contain this section, and it needs it.

| Requirement | Who decides | Applies to |
| --- | --- | --- |
| Institutional biosafety approval | Biosafety committee of the host laboratory | All cell handling |
| Human cell line review | Institutional review board, and the stem cell oversight committee where the institution has one | Any human-derived line; scope depends on whether the donor is identifiable |
| Consent and provenance check | Same bodies, with the cell supplier's documentation | Before a line is received |
| Animal-derived cells | Animal care and use committee | Rodent astrocytes or neurons, as in v0.4's reference preparation |
| Disposal and retirement | Facility procedure | Every cartridge |

A software company cannot hold these approvals alone. The living work has to run inside an institution that has them, under its named investigators.

**22.6 Lines this program does not cross.**

- Nothing is swallowed, injected or implanted, and nothing is tested in a person.
- No transplantation into animals.
- No embryos or embryo-like structures.
- No medical or diagnostic claim.
- No public claim that the device feels, understands or is conscious. The word used in the 2022 DishBrain paper's title drew strong criticism for that reason.

**22.7 Ethics checklist before W3 commissioning**

- [ ] Host institution and responsible investigator named
- [ ] Biosafety approval on file
- [ ] Consent scope of every human line verified against intended use
- [ ] Data protection plan for genomic and recording data
- [ ] Welfare limits from 22.2 written into the controller's limit table
- [ ] Retirement and disposal procedure agreed
- [ ] Plain-language statement of what the project does and does not claim

**Sources for this section:** [National Academies, The Emerging Field of Human Neural Organoids, Transplants, and Chimeras (2021), findings](https://nap.nationalacademies.org/read/26078/chapter/8) and [oversight chapter](https://www.nationalacademies.org/read/26078/chapter/6); [The Transmitter on organoid regulation](https://www.thetransmitter.org/organoids/what-is-the-future-of-organoid-and-assembloid-regulation/); [STAT, November 2025, on calls for international oversight](https://www.statnews.com/2025/11/06/neural-organoid-ethics-global-oversight-needed/). I read search summaries of these, not the full texts, and I am not a lawyer; the host institution's offices give the binding answer.
