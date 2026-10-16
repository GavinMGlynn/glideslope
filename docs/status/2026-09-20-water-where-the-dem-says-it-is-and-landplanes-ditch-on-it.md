### Water where the DEM says it is, and landplanes ditch on it, 2026-09-20 — item done (CI run 35449051367)

Phase 5's "Water where the DEM says it is": the ground under an aircraft is
water - the sea, a lake or a river - where the Copernicus DEM's water body
mask says it is, and there JSBSim is told its surface is not solid, so no
wheel takes weight on it. A landplane that comes down on water ditches: the
step any of its contact points, a wheel or a point of its structure, reaches
the water, it is brought to rest and held there with JSBSim's hold-down,
until it is started again.

**The mask.** Each Copernicus tile is published with a water body mask
beside it, `AUXFILES/..._WBM.tif`, on the same grid: bytes, 0 no water, 1
ocean, 2 lake, 3 river (the Product Handbook's table 8). The GeoTIFF reader
now reads 8-bit unsigned samples and the horizontal differencing predictor
they are published with, in every layout it reads floats in (16 layouts
more, tested), and refuses a float predictor on bytes or a float format on
eight bits by name. `world::Dem::water` gives a place its nearest sample's
value - the next tile's first row or column where that is nearer, the sea
where there is no tile - and refuses a mask with a value the handbook does
not give, or heights in its place. The client fetches each tile's mask as it
fetches the tile, checked against the bucket's MD5.

**Held to.** The pinned mask south of Sydney decodes to what an independent
decoder (Python's zlib, the differencing undone by hand) reads at eighteen
samples. At eleven places at least a kilometre from a shore, a Cessna flown
1,000 ft over each is over water or land as the mask says and the handbook's
class is the place's: the Tasman Sea off Bondi and off Maroubra, ocean;
Maroubra, Sydney airport, Hyde Park and Parramatta, land; Lake Macquarie and
Tuggerah Lake, lakes; Sydney Harbour, Botany Bay and Broken Bay, river - the
mask calls its harbours and bays, where rivers meet the sea, river. And
every one of the fifteen aircraft, set down at idle from 30 ft on water as
on land, ditches where it meets the water, moving not a hundredth of a foot
after, no wheel ever taking its weight, where on land it lands on its wheels
and rolls on.

**Why ditching, and not a landplane floating or sliding.** A first version
let the airframe's structure contact points meet the water, as they meet
the ground. Seven of the fifteen models have none - the 737-300, 747-400,
A320, B-2A, F-15C, F-22A and F-35A - and fell through the water; and of those
that have them, the J-3 Cub, resting on the two points at its wing tips,
rocked between them each step harder than the last until, a second after
touching, the water threw it 125 ft/s upward: point contacts as stiff as a
wing tip's, meeting water with a light aircraft's small inertia in roll, are
beyond a 120 Hz step. How an airframe meets water is hydrodynamics no model
here has, so a landplane ditches, and is still. The flying boat to come has
its hull's hydrodynamics and will not ditch.
