// CAnimBlendSequence - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/Animation/AnimBlendSequence.h
//
// Animation key-frames of a single node (bone).
//
// Adaptations:
// - Stripped InjectHooks(), Constructor()/Destructor() placement wrappers.
// - VALIDATE_SIZE(0xC) -> guarded static_assert (see bottom).
// - `#undef MoveMemory` kept (defensive; Windows headers define it as a macro).
// - eBoneTag16/eBoneTag32 -> AnimTypes.h (WEnum shims); KeyFrame* structs -> AnimTypes.h.
// - ConvertKeyFrames<From,To> kept as a private template declaration
//   (TODO: verify instantiations from the decomp when porting).

#pragma once

#include "AnimTypes.h"

#include <cassert>
#include <cstdint>

#undef MoveMemory

/*!
 * @brief Animation key-frames of a single node (bone)
 */
class CAnimBlendSequence {
public:
    CAnimBlendSequence() = default; // 0x4D0C10
    ~CAnimBlendSequence();

    void CompressKeyframes(uint8_t* frameData);
    [[nodiscard]] size_t GetDataSize(bool compressed) const;
    bool MoveMemory();
    void Print();
    void RemoveQuaternionFlips() const;
    void RemoveUncompressedData(uint8_t* frameData);
    void SetBoneTag(eBoneTag32 boneId);
    auto GetBoneTag() const { assert(m_IsUsingBoneTag); return m_BoneTag; }
    void SetName(const char* string);
    auto GetNameHashKey() const { assert(!m_IsUsingBoneTag); return m_NameHashKey; }
    void SetNumFrames(uint32_t count, bool bHasTranslation, bool compressed, void* frameData);
    bool IsUsingBoneTag() const { return m_IsUsingBoneTag; }
    void Uncompress(uint8_t* frameData);
    bool IsCompressed() const { return m_bIsCompressed; }
    bool HasTranslation() const { return m_bHasTranslation; }

    /*!
     * @brief Used to implement copy-paste functions
     * @tparam AsCompressed Whenever `IsCompressed()` is returning true/false at the runtime.
     * @param n The index of the key-frame
     * @return The `n`th key-frame
    */
    template<bool AsCompressed>
    auto GetKeyFrame(size_t n) const {
        assert(IsCompressed() == AsCompressed); // NOTE: If this is ever hit see the note in `UncompressAnimation`
        if constexpr (AsCompressed) {
            return GetCKeyFrame(n);
        } else {
            return GetUKeyFrame(n);
        }
    }

    /*!
     * @brief Get the `n`th key-frame (Use this only for un-compressed anims (if `!IsCompressed()`))
     * @brief The true return type is `KeyFrameTrans*` (if `HasTranslation()`) or `KeyFrame*`,
     * @brief The former is just returned for convenience.
     * @param n The index of the key-frame
     * @return The `n`th key-frame
    */
    KeyFrameTrans* GetUKeyFrame(size_t n) const { // `U` = Uncompressed
        assert(n < m_FramesNum);
        assert(!m_bIsCompressed); // NOTE: If this is ever hit see the note in `UncompressAnimation`
        return static_cast<KeyFrameTrans*>(m_bHasTranslation ? // Lie a little by always up-casting to `KeyFrameTrans*` to make our lifes easier
            &((KeyFrameTrans*)m_Frames)[n] :
            &((KeyFrame*)m_Frames)[n]
        );
    }

    /*!
    * @brief Get the `n`th key-frame (Use this only for compressed anims (if `IsCompressed()`))
    * @brief The true return type is `KeyFrameTransCompressed*` (if `HasTranslation()`) or `KeyFrameCompressed*`,
    * @brief The former is just returned for convenience.
    * @param n The index of the key-frame
    * @return The `n`th key-frame
    */
    KeyFrameTransCompressed* GetCKeyFrame(size_t n) const { // `C` = Compressed
        assert(n < m_FramesNum);
        assert(m_bIsCompressed); // NOTE: If this is ever hit see the note in `UncompressAnimation`
        return static_cast<KeyFrameTransCompressed*>(m_bHasTranslation ? // Lie a little by always up-casting to `KeyFrameTransCompressed*` to make our lifes easier
            &((KeyFrameTransCompressed*)m_Frames)[n] :
            &((KeyFrameCompressed*)m_Frames)[n]
        );
    }

private:
    /*!
     * @brief Implementation for compression/decompression of key-frames
     * @tparam From Key-Frame type to convert from
     * @tparam To Key-Frame type to convert to
     * @param [opt] frameData Pre-allocated frame data buffer
    */
    template<typename From, typename To>
    bool ConvertKeyFrames(uint8_t* pDataBlock);

private:
    //! The bone/frame this sequence is associated with
    //! Thanks to jte for some info
    union {
        eBoneTag16 m_BoneTag{BONE_UNKNOWN};  // IF m_IsUsingBoneTag == TRUE
        uint32_t   m_NameHashKey;             // IF m_IsUsingBoneTag == FALSE
    };

public:
    uint16_t m_bHasRotation : 1{};
    uint16_t m_bHasTranslation : 1{};      // Root key frames have translation values (quaternion).
    uint16_t m_bIsCompressed : 1{};       // Compressed key frames.
    uint16_t m_bUsingExternalMemory : 1{}; // When this flag is NOT set, you have to loop through all key frames in m_pFrames and free them separately.
    uint16_t m_IsUsingBoneTag : 1{};
    uint16_t m_FramesNum{};
    void*    m_Frames{};
};

#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CAnimBlendSequence) == 0xC, "CAnimBlendSequence layout drift");
#endif
