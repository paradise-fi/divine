module Divine.Blob ( Blob, poolBlob, mallocBlob, pokeBlob, peekBlob ) where

import qualified Divine.Pool as P
import Data.Storable
import Foreign.Storable
import Foreign.Ptr
import Foreign.C.Types
import Foreign.Marshal.Alloc

type Blob = Ptr ()

mkBlob :: (StorableM x) => (Int -> IO Blob) -> x -> IO Blob
mkBlob alloc a = do m <- alloc $ size + header
                    poke (castPtr m) (fromIntegral size :: CShort)
                    pokeBlob m a
                    return m
    where size = sizeOfV a
          header = sizeOf (undefined :: CShort)
{-# INLINE mkBlob #-}

mallocBlob :: (StorableM x) => x -> IO Blob
mallocBlob = mkBlob mallocBytes
{-# INLINE mallocBlob #-}

poolBlob :: (StorableM x) => P.Pool -> x -> IO Blob
poolBlob p = mkBlob (P.alloc p)
{-# INLINE poolBlob #-}

pokeBlob :: (StorableM y) => Blob -> y -> IO ()
pokeBlob blob v = pokeV (blob `plusPtr` 2) v

peekBlob :: (StorableM y) => Blob -> IO y
peekBlob blob = peekV (blob `plusPtr` 2)
{-# INLINE peekBlob #-}
