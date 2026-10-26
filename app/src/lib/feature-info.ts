/**
 * What each feature does, with its pros and cons, for the info buttons
 * beside it (InfoTip).
 */

export interface FeatureInfo {
  title: string;
  what: string;
  pros: string[];
  cons: string[];
}

export const FEATURE_INFO = {
  presets: {
    title: "Presets",
    what: "Named sets of settings you can switch between in one click. Doghouse keeps your own presets, so they work for every board in your pack.",
    pros: ["Swap between typing and gaming without hunting through settings.", "Keeps your tables (combos, tap dances) even when a preset switches them off."],
    cons: ["Applied live like any change: use Save to board to keep it after a power-off.", "Doesn't touch the keymap, macros or light colours."],
  },
  debounce: {
    title: "Debounce",
    what: "Switch contacts bounce for a few milliseconds when pressed or released. Debounce is how long the board ignores that bouncing.",
    pros: ["A higher value stops worn or noisy switches from double-typing.", "A lower value lets you tap the same key again sooner."],
    cons: ["Too low and a key can type twice.", "With a wait-to-settle style, every millisecond here is added to every key press."],
  },
  debounceStyle: {
    title: "Debounce style",
    what: "Eager styles send a press the moment a switch closes, then ignore bouncing. Wait-to-settle styles send it only once the switch has been steady for the debounce time.",
    pros: ["Eager: the lowest latency; the press reaches the computer straight away.", "Wait to settle: ignores electrical noise, so a stray blip never types anything."],
    cons: ["Eager: a noisy switch or a static zap can register as a press.", "Wait to settle: adds the debounce time to every key press."],
  },
  tappingTerm: {
    title: "Tapping term",
    what: "How long you can hold a dual-role key (layer-tap, mod-tap, tap dance) and still have it count as a tap.",
    pros: ["Longer: fewer accidental holds when you type slowly.", "Shorter: holds kick in sooner, so shortcuts feel snappier."],
    cons: ["Longer: you wait longer for the hold action.", "Shorter: a relaxed tap can turn into a hold by mistake."],
  },
  nkro: {
    title: "N-key rollover",
    what: "Reports every key held at once, instead of the six the classic keyboard report allows.",
    pros: ["No dropped keys when many are held together, as in games and fast typing."],
    cons: ["Some BIOS screens, KVM switches and old computers only understand six keys; turn it off for them."],
  },
  sleep: {
    title: "Nap after",
    what: "On battery, the board goes to sleep after this long without a key press. Any key wakes it.",
    pros: ["Shorter naps make the battery last much longer."],
    cons: ["The first key after a nap takes a moment to wake the board and reconnect, and may be lost over Bluetooth."],
  },
  brightness: {
    title: "Indicator brightness",
    what: "How bright the layer, battery and lock lights are.",
    pros: ["Brighter lights are easier to see in daylight."],
    cons: ["Brighter lights drain the battery faster."],
  },
  layerLight: {
    title: "Layer light",
    what: "A colour for each layer, shown while that layer is the highest one on.",
    pros: ["You always know which layer you're typing on."],
    cons: ["A light that's on uses a little battery; black keeps it off."],
  },
  lockLights: {
    title: "Lock lights",
    what: "Show Caps Lock, Num Lock or Scroll Lock while the computer has it on.",
    pros: ["No more typing a sentence in capitals by accident."],
    cons: ["Uses a little battery while lit."],
  },
  batteryGauge: {
    title: "Battery gauge",
    what: "The light that shows how much charge is left, and the colour it turns when the battery runs low.",
    pros: ["A glance tells you when to plug in."],
    cons: ["Another light to power, though only briefly."],
  },
  lowBattery: {
    title: "Low battery level",
    what: "The charge at which the battery gauge turns to its low colour.",
    pros: ["A higher level warns you sooner."],
    cons: ["Too high and it nags while there's plenty left."],
  },
  factoryReset: {
    title: "Factory reset",
    what: "Puts back the default keymap, settings, advanced keys and macros, and saves them.",
    pros: ["A clean start if something is set up in a way you can't untangle."],
    cons: ["Everything you've changed on the board is lost. Back it up first (About tab)."],
  },
  tapHoldSwitch: {
    title: "Tap-hold keys",
    what: "Layer-tap and mod-tap keys do one thing when tapped and another when held. Switched off, they always act as their tap key.",
    pros: ["On: one key does two jobs, like Space that's also a layer key, or home-row mods.", "Off: the key goes down the instant you press it, with no waiting to see if it's a hold."],
    cons: ["On: the tap waits until you let go (or the tapping term passes), which adds latency.", "Off: their hold actions (layers, modifiers) are out of reach."],
  },
  tapDanceSwitch: {
    title: "Tap dance",
    what: "TD keys do different things for a tap, a hold, a double tap or a triple tap. Switched off, they act as their single tap at once.",
    pros: ["On: up to four jobs on one key.", "Off: no waiting for a possible second tap; the key acts the moment it goes down."],
    cons: ["On: a single tap only counts once the tapping term has passed with no second tap.", "Off: the hold, double and triple tap actions are out of reach."],
  },
  combosSwitch: {
    title: "Combos",
    what: "Pressing a few keys together sends something else. Switched off, those keys always type themselves.",
    pros: ["On: extra keys without moving your hands.", "Off: keys used in combos stop waiting to see if their partner follows."],
    cons: ["On: a key that starts a combo is held back for up to the combo window (50 ms by default).", "Off: combos don't fire."],
  },
  leaderTimeout: {
    title: "Leader timeout",
    what: "After the Leader key, how long you have for each key of a sequence.",
    pros: ["Longer: no rush when typing a sequence."],
    cons: ["Longer: when a longer sequence could still follow, its result arrives later after you finish."],
  },
  capsWord: {
    title: "Caps Word",
    what: "Types one word in capitals (minus becomes underscore), then turns itself off at the end of the word: a space or most punctuation.",
    pros: ["Great for CONSTANT_NAMES and acronyms; no Caps Lock to forget."],
    cons: ["Tapping both Shifts together may happen by accident when typing fast."],
  },
  capsWordIdle: {
    title: "Caps Word idle timeout",
    what: "Caps Word switches off after this long without typing.",
    pros: ["It never lingers on while you think."],
    cons: ["Pause too long mid-word and the rest comes out lower case."],
  },
  autoShift: {
    title: "Auto Shift",
    what: "Hold a letter, number or symbol key a little longer and it comes out shifted, so you rarely need Shift.",
    pros: ["Less pinky strain from Shift."],
    cons: ["Every such key waits until you let go (or the hold time passes) before typing, which adds latency and makes key repeat awkward."],
  },
  autoShiftMs: {
    title: "Auto Shift hold time",
    what: "How long to hold a key for its shifted version.",
    pros: ["Shorter: shifted keys come quicker."],
    cons: ["Shorter: slow typists get capitals they didn't want."],
  },
  mouseSpeed: {
    title: "Mouse keys speed",
    what: "How far the pointer moves each step when you start, and how fast it can go once it has sped up.",
    pros: ["A low start speed for precise moves, a high top speed to cross the screen."],
    cons: ["Too high and the pointer overshoots."],
  },
  mouseAccel: {
    title: "Mouse keys acceleration",
    what: "How long a mouse key takes to reach top speed.",
    pros: ["Shorter: crosses the screen sooner."],
    cons: ["Shorter: harder to make small, precise moves."],
  },
  mouseInterval: {
    title: "Mouse step interval",
    what: "Time between pointer movement steps.",
    pros: ["Shorter: smoother movement."],
    cons: ["Shorter: more reports, a little more battery."],
  },
  wheelInterval: {
    title: "Scroll step interval",
    what: "Time between scroll steps while a wheel key is held.",
    pros: ["Shorter: scrolls faster."],
    cons: ["Shorter: harder to scroll just one line."],
  },
  combos: {
    title: "Combos",
    what: "Press a few keys together to send something else, like J + K for Esc.",
    pros: ["Extra keys without moving your hands or adding layers."],
    cons: ["Keys that start a combo are held back for up to the combo window, adding latency to those keys. Gaming presets switch combos off."],
  },
  tapDances: {
    title: "Tap dances",
    what: "One key with up to four jobs: tap, hold, double tap and triple tap. Put TD(n) on a key to use one.",
    pros: ["Packs a lot onto a small board."],
    cons: ["A single tap waits for the tapping term in case a second tap follows. Gaming presets switch tap dance off."],
  },
  keyOverrides: {
    title: "Key overrides",
    what: "Change what a key sends while certain modifiers are held, like Shift + Backspace for Delete.",
    pros: ["New shortcuts without giving up a key."],
    cons: ["Can surprise you in apps that use that shortcut for something else."],
  },
  leader: {
    title: "Leader sequences",
    what: "Tap the Leader key, then a short sequence of keys, to send a key or shortcut.",
    pros: ["Lots of shortcuts that are easy to remember, without holding anything."],
    cons: ["Slower than a direct key, and the keys you type in a sequence don't type themselves."],
  },
  tapHold: {
    title: "Tap-hold rules",
    what: "Fine-tune one layer-tap or mod-tap key: its own tapping term, and how it decides between tap and hold.",
    pros: ["Make home-row mods reliable key by key."],
    cons: ["More to tune; a rule that's too eager gives accidental holds."],
  },
  macros: {
    title: "Macros",
    what: "Put M0, M1 and so on on a key and it plays its macro: typed text, key taps, held keys and pauses.",
    pros: ["Type a long phrase or a fiddly shortcut with one key."],
    cons: ["Text is typed as a US layout would, so other layouts may get the wrong symbols.", "Some games treat macros as cheating."],
  },
  outputs: {
    title: "Where keys go",
    what: "Which connection the board types over when more than one computer is listening: the USB cable, the 2.4 GHz dongle or Bluetooth.",
    pros: ["USB: the lowest latency and no battery use.", "2.4 GHz dongle: close to wired latency, no cable.", "Bluetooth: no dongle needed, and up to three computers."],
    cons: ["USB: a cable on the desk.", "2.4 GHz dongle: uses a USB port, and only one computer.", "Bluetooth: more latency than the dongle, and the first key after a nap can be slow."],
  },
} as const satisfies Record<string, FeatureInfo>;

export type FeatureInfoId = keyof typeof FEATURE_INFO;
