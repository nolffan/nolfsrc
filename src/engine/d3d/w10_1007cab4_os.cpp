// *********************************************************************** //
//
//  w10_1007cab4_os.cpp -- d3d.ren's model-animation setup and blend
//  members of D3DRenderInfo (big7cc90.cpp's name for the class), written
//  for the whole-DLL link.  /Os group (w<N>_*_os.cpp).
//
//  WHY /Os: 0x1007cab4, 0x1007d624 and 0x1007da80 end with the /Os
//  padding (`mov %esi,%esi` then single 0x90s).
//
//  THE /Qip SHIMS.  The shipped TU was built /Qip as well: 0x1007cab0 and
//  0x1007d620 are four-byte stack entries (`mov 0x4(%esp),%eax`) falling
//  into the register entries 0x1007cab4 / 0x1007d624.  Neither register
//  entry reads %eax -- both re-read their arguments from the frame -- so
//  the shims carry nothing and the functions are written as ordinary
//  __thiscall members, named at the register-entry address and measured
//  from there.  Callers in our other TUs (d3dr4a.cpp's 0x1007ca60,
//  big7cc90.cpp's 0x1007cc90) reach them as ordinary __thiscall members,
//  which is what they are once the shim is gone.
//
//  The four functions are
//  rewritten from their lithtech.exe twin (src/engine/lithtech/
//  transform.cpp: Prepare, BlendNode, GetNodeTransformRel,
//  GetNodeTransform) with the real LTRotation / LTVector, and the model's
//  arrays are dynarray.h's REAL CMoArray: the anim table at
//  model +0x164, the per-node key arrays at +0x00 of a node set and +0x08
//  of a node, the weights at +0x10 of a weight set.  CMoArray's inline
//  operator[] is what puts the array base first in the shipped address
//  modes (`(%edi,%eax,1)`, not `(%eax,%edi,1)`).  0x1007cab4 and
//  0x1007da80 are identical; see each function for the rest.  The model's
//  weight-set array at +0x44 stays a hand pair: as a CMoArray it costs
//  0x1007cab4 (466 -> 297 bytes off).  0x1007da80's new signature is
//  aliased for big7cc90.cpp's old spelling in link/aliases.rsp.
//
// *********************************************************************** //

#include "ltbasedefs.h"
#define LAlloc		IAllocator
#define g_DefAlloc	g_10091df0
#include "../../LT2/lithshared/stdlith/dynarray.h"
#undef LAlloc
#undef g_DefAlloc


struct LTMat
{
	float  m[4][4];
};


// ----------------------------------------------------------------------- //
//  The model's 0x20-byte animation table at +0x168 (the stride is the
//  `imul $0x20` at 0x1007cb63 / 0x1007db7c).  +0x00 is the per-node key
//  table (0x1007cb66 -> +0x208/+0x228), +0x04 the base pose (0x1007cbee
//  -> +0x144), +0x14 the animation's translation offset (0x1007db82).
// ----------------------------------------------------------------------- //
struct D3DAnimNodeKeys;

struct D3DAnimKeyTable                                  // model +0x168[i]
{
	D3DAnimNodeKeys*	m_pNodeKeys00;                  // +0x00
	void*				m_pBasePose04;                  // +0x04
	uint8				m_pad_08[0x0c];                 // +0x08
	LTVector			m_vOffset14;                    // +0x14
};

struct D3DModel
{
	uint8				m_pad_000[0x48];                // +0x000
	struct D3DNodeWeights**	m_ppWeights48;              // +0x048
	uint32				m_nWeightSets4c;                // +0x04c
	uint8				m_pad_050[0x74 - 0x50];         // +0x050
	LTMat*				m_pDefNodeMats74;               // +0x074
	uint8				m_pad_078[0x164 - 0x78];        // +0x078
	CMoArray<D3DAnimKeyTable>	m_Anims164;             // +0x164 (array +0x168)
};

// One keyframe: a position and a rotation, 0x1c bytes (0x1007dac9 /
// 0x1007dadc `imul $0x1c`; the rotation at +0x0c, 0x1007dacf/0x1007daf2).
struct D3DKeyFrame
{
	LTVector	m_vPos00;                               // +0x00
	LTRotation	m_Rot0c;                                // +0x0c
};

struct D3DNodeKeyArray
{
	uint8					m_pad_00[0x08];             // +0x00
	CMoArray<D3DKeyFrame>	m_Keys08;                   // +0x08 (array +0x0c)
};

// Per-node key arrays: +0x04 is indexed by node (0x1007daa3).
struct D3DAnimNodeKeys
{
	CMoArray<D3DNodeKeyArray*>	m_Nodes00;              // +0x00 (array +0x04)
};

// Per-node blend weights: +0x14 is a float per node (0x1007d646).
struct D3DNodeWeights
{
	uint8			m_pad_00[0x10];                     // +0x00
	CMoArray<float>	m_Weights10;                        // +0x10 (array +0x14)
};


// ----------------------------------------------------------------------- //
//  The 0x1c-byte animation record, eight of them at +0x000.
//      +0x00  the model it animates (0x1007cad1, 0x1007caf1)
//      +0x04  animation A, a WORD index into model +0x168 (0x1007cb52)
//      +0x06  keyframe in A (0x1007dac4)
//      +0x0c  animation B (0x1007cb6f)
//      +0x0e  keyframe in B (0x1007dad7)
//      +0x14  the A->B interpolant (0x1007daea, 0x1007db0f)
//      +0x18  weight-set index into model +0x48 (0x1007cb05, 0x1007cbb1)
//  0x10073640 is its validity test (d3dr5c.cpp), called __thiscall on
//  each record (0x1007cae8).
// ----------------------------------------------------------------------- //
struct D3DAnimRecord
{
	D3DModel*	m_pModel00;                             // +0x00
	uint16		m_iAnimA04;                             // +0x04
	uint16		m_iKeyA06;                              // +0x06
	uint8		m_pad_08[0x04];                         // +0x08
	uint16		m_iAnimB0c;                             // +0x0c
	uint16		m_iKeyB0e;                              // +0x0e
	uint8		m_pad_10[0x04];                         // +0x10
	float		m_fPercent14;                           // +0x14
	uint32		m_iWeightSet18;                         // +0x18

	uint32		sub_10073640() const;                   // 0x10073640
};


class D3DRenderInfo
{
public:
	uint32	sub_1007cab4();
	uint32	IsValid();
	void	sub_1007d624(uint32 iAnim, uint32 iNode);
	void	sub_1007da80(uint32 iAnim, uint32 iNode, LTRotation* pRot, LTVector* pPos);
	void	sub_1007d880(uint32 iAnim, uint32 iNode, LTRotation* pRot, LTVector* pPos);

	D3DAnimRecord		m_Anims[8];                     // +0x000
	uint32				m_nNumAnims;                    // +0x0e0
	LTMat*				m_pRootXForme4;                 // +0x0e4
	LTMat*				m_pNodeMatse8;                  // +0x0e8
	void*				m_pfnCallbackec;                // +0x0ec
	void*				m_pUserf0;                      // +0x0f0
	void*				m_pObjectf4;                    // +0x0f4
	uint32*				m_pIndexStackf8;                // +0x0f8
	uint32				m_nIndexStackfc;                // +0x0fc
	void*				m_pCurXForm100;                 // +0x100
	LTMat				m_Mat104;                       // +0x104
	void*				m_pBasePose144;                 // +0x144
	LTMat				m_Mat148;                       // +0x148
	LTRotation			m_Rot188;                       // +0x188
	LTVector			m_Vec198;                       // +0x198
	D3DModel*			m_pModel1a4;                    // +0x1a4
	LTMat				m_Identity1a8;                  // +0x1a8
	D3DNodeWeights*		m_pWeights1e8[8];               // +0x1e8
	D3DAnimNodeKeys*	m_pKeysA208[8];                 // +0x208
	D3DAnimNodeKeys*	m_pKeysB228[8];                 // +0x228
};



// 0x10073500 -- quaternion slerp (dest, from, to, t), __cdecl: every call
// site hands it four dwords in an outgoing area (0x1007dafe, 0x1007d6d0).
// w10_10073400_osoiqip.cpp defines it over its own LTQuat struct, whose
// mangled spelling this keeps; ltquatbase.h's quat_Slerp takes float*.
struct LTQuat;
extern void sub_10073500(LTQuat* pDest, LTQuat* pFrom, LTQuat* pTo, float t);

// LTRotation::Slerp (ltrotation.h), the inline that hands quat_Slerp its
// float t as a parameter.
inline void RotSlerp(LTRotation* pDest, const LTRotation& q1, const LTRotation& q2, float t)
{
	sub_10073500((LTQuat*)pDest, (LTQuat*)&q1, (LTQuat*)&q2, t);
}


// ======================================================================= //
//  0x1007cab4, 464 bytes (register entry of 0x1007cab0).  __thiscall, no
//  arguments, returns 1 on success and 0 on failure.  1 caller, 0x1007ca60
//  (d3dr4a.cpp), which fails the whole setup on 0.
//
//  Validates the animation records and resolves their tables:
//    * 1..8 records (0x1007cac6 / 0x1007caca);
//    * each record valid (0x10073640), animating the SAME model as the
//      first (0x1007caf1), and every record after the first naming a
//      weight set the model has (0x1007cb0b, `jae` -> fail);
//    * then the model is published at +0x1a4, +0xf8 is cleared, and per
//      record the A and B key tables and (after the first) the weight set
//      are looked up (0x1007cb52..0x1007cbba -- the i != 0 test is rotated
//      to the loop's bottom, and the weight-set arm is the one placed out
//      of line);
//    * a NULL root transform is replaced by an identity matrix built in
//      place at +0x1a8 (the 1.0f stores 0x1e4/0x1d0/0x1bc/0x1a8);
//    * a NULL node-matrix array defaults to the model's (+0x74);
//    * the base pose is animation A's of the first record.
//
//  IDENTICAL (was 263 bytes off).  The inline
//  IsValid removes the return trampolines.  Then: the anim table is a
//  CMoArray (the base-first address modes), and the weight-set test is
//  spelled `if (i == 0) ; else ...`.  `if (i > 0)`, `if (i)`, `i != 0`
//  and `0 < i` all put the weight-set arm inline and the i == 0 path out
//  of line (the reverse of shipped); `i >= 1` / `i < 1` get the layout but
//  emit `cmp $0x1` for the shipped `test`.
// ======================================================================= //
inline uint32 D3DRenderInfo::IsValid()
{
	D3DModel* pModel;
	uint32 i;

	if (m_nNumAnims == 0 || m_nNumAnims > 8)
		return 0;

	pModel = m_Anims[0].m_pModel00;

	for (i = 0; i < m_nNumAnims; i++)
	{
		if (!m_Anims[i].sub_10073640())
			return 0;

		if (m_Anims[i].m_pModel00 != pModel)
			return 0;

		if (i != 0 && m_Anims[i].m_iWeightSet18 >= pModel->m_nWeightSets4c)
			return 0;
	}

	return 1;
}

uint32 D3DRenderInfo::sub_1007cab4()
{
	uint32 i;

	if (!IsValid())
		return 0;

	m_pModel1a4 = m_Anims[0].m_pModel00;
	m_pIndexStackf8 = LTNULL;

	for (i = 0; i < m_nNumAnims; i++)
	{
		if (i == 0)
			;	// the first record's weight set is never used
		else
			m_pWeights1e8[i] = m_pModel1a4->m_ppWeights48[m_Anims[i].m_iWeightSet18];

		m_pKeysA208[i] = m_pModel1a4->m_Anims164[m_Anims[i].m_iAnimA04].m_pNodeKeys00;
		m_pKeysB228[i] = m_pModel1a4->m_Anims164[m_Anims[i].m_iAnimB0c].m_pNodeKeys00;
	}

	if (!m_pRootXForme4)
	{
		m_Identity1a8.m[0][0] = 1.0f; m_Identity1a8.m[0][1] = 0.0f; m_Identity1a8.m[0][2] = 0.0f; m_Identity1a8.m[0][3] = 0.0f;
		m_Identity1a8.m[1][0] = 0.0f; m_Identity1a8.m[1][1] = 1.0f; m_Identity1a8.m[1][2] = 0.0f; m_Identity1a8.m[1][3] = 0.0f;
		m_Identity1a8.m[2][0] = 0.0f; m_Identity1a8.m[2][1] = 0.0f; m_Identity1a8.m[2][2] = 1.0f; m_Identity1a8.m[2][3] = 0.0f;
		m_Identity1a8.m[3][0] = 0.0f; m_Identity1a8.m[3][1] = 0.0f; m_Identity1a8.m[3][2] = 0.0f; m_Identity1a8.m[3][3] = 1.0f;
		m_pRootXForme4 = &m_Identity1a8;
	}

	if (!m_pNodeMatse8)
		m_pNodeMatse8 = m_pModel1a4->m_pDefNodeMats74;

	m_pBasePose144 = m_pModel1a4->m_Anims164[m_Anims[0].m_iAnimA04].m_pBasePose04;

	return 1;
}


// ======================================================================= //
//  0x1007d624, 591 bytes (register entry of 0x1007d620).  __thiscall
//  (iAnim, iNode), `ret $0x8`.  1 caller, 0x1007cd0e (big7cc90.cpp's
//  0x1007cc90), once per record after the first.
//
//  Blends animation record iAnim into the node's running transform
//  (+0x188 rotation, +0x198 position) by the node's weight in the
//  record's weight set (+0x1e8[iAnim]->+0x14[iNode]):
//    * weight 0.0f (0x100abfa0): nothing;
//    * weight 2.0f (0x100abfa4) selects the ADDITIVE blend: evaluate
//      relative to the animation's first keyframe with 0x1007d880 and
//      compose -- rotation by quaternion product (+0x188 = +0x188 * q,
//      ltquatbase.h's quat_Mul expanded into a temporary and copied back,
//      0x1007d753..0x1007d84b) and position by sum (+0x198 + v);
//    * anything else: evaluate with 0x1007da80, slerp the running rotation
//      toward it by the weight (0x10073500, from a COPY of +0x188), and
//      lerp the running position toward it.  The weight is ONE float
//      local: icl keeps it in an x87 register and spills that as a double
//      (`fstpl -0x8(%ebp)` / `fmull -0x8(%ebp)`); a declared double would
//      realign the stack (`and $0xfffffff0,%esp`), which 0x1007d624 does
//      not.
//
//  IDENTICAL, with the product through quat_Mul_1007d624 below (the
//  header's quat_Mul with two operands of its first row addressed
//  through the vector part).  Before that:
//  NEAR, 23 bytes (184 -> 23 with the real LTRotation/LTVector;
//  re-measured).  The residual is x87 scheduling inside the inlined
//  quat_Mul of `m_Rot188 * rot`: in the out[QX] row shipped loads a[QW]
//  (0x194) and multiplies by b[QX], ours the other way round, which moves
//  the store of out[QY] by two instructions.  No effect: an explicit
//  quat_Mul into a temp, `LTRotation* pR = &m_Rot188`, a named product
//  temp, the order of the two accumulate statements, `+=`, the local
//  declaration order, CMoArray<float> weights.  (Also: /G5, /G6, /Qip,
//  operand and term order, file name, preceding code.)
//  With ltquatbase.h's quat_Mul copied into a local inline
//  so its terms could be permuted (both functions measured together):
//  the SDK text reproduces 23 / 39 exactly.  The operand order INSIDE a
//  product has no effect at all (all 16 swaps of out[QX], any term
//  order) -- icl orders each product's loads itself -- and of the 18
//  out[QX] term orders and 24 row orders the SDK's is the best for both
//  (next 25 / 63, 26 / 37).  Also unchanged: `const LTRotation&` for
//  either operand, `operator*` called by name, RotSlerp / the m_Quat
//  spelling for the slerp arm, a pSet local for the weights, the weight
//  initialised at its declaration, the definitions reordered (0x1007d880
//  and 0x1007da80 first), and the file renamed (stems of 8..28
//  characters).  Worse: the arms swapped (`!= 2.0f` first, 43), rot /
//  vPos / rOld scoped to their arms (554), the two accumulate statements
//  swapped (262), `m_Vec198 += vPos` (149).
// ======================================================================= //
// ltquatbase.h's quat_Mul with the two x components of the out[QX] row
// read through pointers to the quaternions' VECTOR parts (the header's own
// note writes a quaternion as [s, v]).  icl sorts each product's operands
// by their address forms, and with the header's text the shipped
// `flds a[QW] ; fmuls b[QX]` comes out the other way round.  Found by
// spelling a[QW], b[QX], a[QX], b[QW] of that row five ways each (625
// spellings, 10 identical -- every one with b[QX] and a[QX] through a
// pointer formed as `&q[QX]` / `q + QX`; a plain copy of the pointer does
// nothing).
inline void quat_Mul_1007d624(float *out, const float *a, const float *b)
{
	const float* av = &a[QX];
	const float* bv = &b[QX];

	out[QX] = a[QW]*bv[0] + av[0]*b[QW] + a[QY]*b[QZ] - a[QZ]*b[QY];
	out[QY] = a[QW]*b[QY] - a[QX]*b[QZ] + a[QY]*b[QW] + a[QZ]*b[QX];
	out[QZ] = a[QW]*b[QZ] + a[QX]*b[QY] - a[QY]*b[QX] + a[QZ]*b[QW];
	out[QW] = a[QW]*b[QW] - a[QX]*b[QX] - a[QY]*b[QY] - a[QZ]*b[QZ];
}

inline LTRotation RotMul_1007d624(const LTRotation& a, const LTRotation& b)
{
	LTRotation qTemp;
	quat_Mul_1007d624(qTemp.m_Quat, a.m_Quat, b.m_Quat);
	return qTemp;
}

void D3DRenderInfo::sub_1007d624(uint32 iAnim, uint32 iNode)
{
	float      fWeight;
	LTRotation rot, rOld;
	LTVector   vPos;

	fWeight = m_pWeights1e8[iAnim]->m_Weights10[iNode];
	if (fWeight == 0.0f)
		return;

	if (fWeight == 2.0f)
	{
		sub_1007d880(iAnim, iNode, &rot, &vPos);

		m_Rot188 = RotMul_1007d624(m_Rot188, rot);
		m_Vec198 = m_Vec198 + vPos;
	}
	else
	{
		sub_1007da80(iAnim, iNode, &rot, &vPos);

		rOld = m_Rot188;
		sub_10073500((LTQuat*)&m_Rot188, (LTQuat*)&rOld, (LTQuat*)&rot, fWeight);

		m_Vec198 = m_Vec198 + (vPos - m_Vec198) * fWeight;
	}
}


// ======================================================================= //
//  0x1007d880, 502 bytes.  __thiscall (iAnim, iNode, pRot, pPos),
//  `ret $0x10`.  1 caller, 0x1007d74e above (the additive arm).
//
//  0x1007da80's evaluation made RELATIVE to the animation's first
//  keyframe: after the same slerp and lerp (0x1007d88b..0x1007d965,
//  instruction for instruction the head of 0x1007da80), the rotation is
//  pre-multiplied by the conjugate of key 0's rotation (the three `fchs`
//  at 0x1007d96a, w copied as an integer) and key 0's position is
//  subtracted from the position.  Key 0 is the base of animation A's
//  node key array, read into %ebp at 0x1007d8f3.
//
//  NEAR, 39 bytes (was 461).  The head is 0x1007da80's (two-element
//  node and key arrays, see there); the key-0 base is its own statement
//  AFTER the two keys, which is what reloads it into %ebp before the
//  call.  The residual is the inlined quat_Mul's x87 operand order, as in
//  0x1007d624: in out[QX] shipped loads a[QY] then multiplies by b[QZ],
//  ours the reverse, and three later loads follow.  Conjugate() instead
//  of `~`, a named conjugate, the four-float constructor, an explicit
//  quat_Mul, and a separate TU holding only 0x1007d880/0x1007da80 leave it
//  unchanged; copying *pRot to a local first costs 433 bytes.
// ======================================================================= //
void D3DRenderInfo::sub_1007d880(uint32 iAnim, uint32 iNode,
                                 LTRotation* pRot, LTVector* pPos)
{
	D3DNodeKeyArray* pNodes[2];
	D3DKeyFrame*     pKeys[2];
	D3DKeyFrame*     pBase;
	D3DAnimRecord*   pRef   = &m_Anims[iAnim];

	pNodes[0] = m_pKeysA208[iAnim]->m_Nodes00[iNode];
	pNodes[1] = m_pKeysB228[iAnim]->m_Nodes00[iNode];
	pKeys[0]  = &pNodes[0]->m_Keys08[pRef->m_iKeyA06];
	pKeys[1]  = &pNodes[1]->m_Keys08[pRef->m_iKeyB0e];
	pBase     = &pNodes[0]->m_Keys08[0];

	RotSlerp(pRot, pKeys[0]->m_Rot0c, pKeys[1]->m_Rot0c, pRef->m_fPercent14);

	*pPos = pKeys[0]->m_vPos00 + (pKeys[1]->m_vPos00 - pKeys[0]->m_vPos00) * pRef->m_fPercent14;

	*pRot = ~pBase->m_Rot0c * *pRot;

	*pPos = *pPos - pBase->m_vPos00;
}



// ======================================================================= //
//  0x1007da80, 336 bytes.  __thiscall (iAnim, iNode, pRot, pPos),
//  `ret $0x10`.  2 callers, both same-TU: 0x1007cce1 (big7cc90.cpp's
//  0x1007cc90) and 0x1007d689 below.
//
//  Evaluates animation record iAnim for one node: slerps the node's
//  keyframe rotations between animations A and B (0x10073500, t = +0x14)
//  and lerps their positions, into *pRot / *pPos.  For the FIRST record
//  node only (iNode == 0, 0x1007db58) it also adds the lerp of the two
//  animations' translation offsets (+0x14 of their table entries).
//
//  The lerp is written `a + (b - a) * t` componentwise into a local
//  vector that is then copied out with `rep movsl` (ecx = 3) -- a vector
//  temporary assigned whole, 0x1007db3d..0x1007db52.
//
//  IDENTICAL (was 277 bytes off).  What did it:
//    * the A/B node pointers and the A/B key pointers are two-element
//      LOCAL ARRAYS: the shipped head stores all four (0x14..0x20(%esp))
//      though the node pair is never read back -- icl keeps stores into
//      an array and forwards the loads, but drops dead scalar stores --
//      and it reloads the A key twice from its slot (277 -> 153);
//    * the key and node arrays are CMoArrays (base-first address modes);
//    * the node-0 block falls through and the `iNode != 0` exit jumps
//      straight to the shared epilogue.  Every `if` spelling (`== 0`, `!`,
//      `<= 0`, `< 1`, an early return, goto, do/while(0), switch on iNode
//      with the block in a case) puts the block out of line; the switch on
//      the comparison whose non-zero arm leaves (0x10040cc0's lever,
//      w7_10040cc0.cpp) is what icl sinks, and only `goto` as that arm
//      avoids a `jmp` trampoline to the epilogue (`return` leaves one);
//    * the node-0 block reads the model's anim array twice (operator[]
//      twice through one pModel local), as shipped.
// ======================================================================= //
void D3DRenderInfo::sub_1007da80(uint32 iAnim, uint32 iNode,
                                 LTRotation* pRot, LTVector* pPos)
{
	D3DNodeKeyArray* pNodes[2];
	D3DKeyFrame*     pKeys[2];
	D3DAnimRecord*   pRef   = &m_Anims[iAnim];

	pNodes[0] = m_pKeysA208[iAnim]->m_Nodes00[iNode];
	pNodes[1] = m_pKeysB228[iAnim]->m_Nodes00[iNode];
	pKeys[0]  = &pNodes[0]->m_Keys08[pRef->m_iKeyA06];
	pKeys[1]  = &pNodes[1]->m_Keys08[pRef->m_iKeyB0e];

	RotSlerp(pRot, pKeys[0]->m_Rot0c, pKeys[1]->m_Rot0c, pRef->m_fPercent14);

	*pPos = pKeys[0]->m_vPos00 + (pKeys[1]->m_vPos00 - pKeys[0]->m_vPos00) * pRef->m_fPercent14;

	switch (iNode != 0)
	{
		case 0:  break;
		default: goto done;
	}
	{
		D3DModel* pModel = m_pModel1a4;
		LTVector& vPrev  = pModel->m_Anims164[pRef->m_iAnimA04].m_vOffset14;
		LTVector& vCur   = pModel->m_Anims164[pRef->m_iAnimB0c].m_vOffset14;

		*pPos += vPrev + (vCur - vPrev) * pRef->m_fPercent14;
	}
done:
	;
}
