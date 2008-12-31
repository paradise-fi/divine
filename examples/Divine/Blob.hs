{-# LANGUAGE ScopedTypeVariables #-}
{-# LANGUAGE PatternSignatures #-}

module Divine.Blob ( Blob, poolBlob, pokeBlob, peekBlob ) where

import qualified Divine.Pool as P
import Foreign.Storable
import Foreign.Ptr
import Foreign.C.Types
import Foreign.Marshal.Alloc

type Blob = Ptr ()

mkBlob :: forall x. (Storable x) => (Int -> IO Blob) -> x -> IO Blob
mkBlob alloc a = do m :: Blob <- alloc $ size + header
                    poke (castPtr m) (fromIntegral size :: CShort)
                    pokeBlob m a
                    return m
    where size = sizeOf (undefined :: x)
          header = sizeOf (undefined :: CShort)
{-# INLINE mkBlob #-}

mallocBlob :: forall x. (Storable x) => x -> IO Blob
mallocBlob = mkBlob mallocBytes
{-# INLINE mallocBlob #-}

poolBlob :: forall x. (Storable x) => P.Pool -> x -> IO Blob
poolBlob p = mkBlob (P.alloc p)
{-# INLINE poolBlob #-}

pokeBlob :: (Storable y) => Blob -> y -> IO ()
pokeBlob blob v = pokeByteOff blob 2 v

peekBlob :: (Storable y) => Blob -> IO y
peekBlob blob = peekByteOff blob 2
