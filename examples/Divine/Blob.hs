module Divine.Blob ( Blob, poolBlob, mallocBlob, pokeBlob, peekBlob ) where

import qualified Divine.Pool as P
import Data.Storable
import Foreign.Storable
import Foreign.Ptr
import Foreign.C.Types
import Foreign.Marshal.Alloc

type Blob = Ptr ()

mkBlob :: (StorableM x) => (Int -> IO Blob) -> Int -> x -> IO Blob
mkBlob alloc slack a = do m <- alloc $ size + header
                          poke (castPtr m) (fromIntegral size :: CShort)
                          pokeBlob m slack a
                          return m
    where size = sizeOfV a + slack
          header = sizeOf (undefined :: CShort)
{-# INLINE mkBlob #-}

mallocBlob :: (StorableM x) => Int -> x -> IO Blob
mallocBlob = mkBlob mallocBytes
{-# INLINE mallocBlob #-}

poolBlob :: (StorableM x) => P.Pool -> Int -> x -> IO Blob
poolBlob p = mkBlob (P.alloc p)
{-# INLINE poolBlob #-}

pokeBlob :: (StorableM y) => Blob -> Int -> y -> IO ()
pokeBlob blob slack v = pokeV (blob `plusPtr` (slack + 4)) v

peekBlob :: (StorableM y) => Blob -> Int -> IO y
peekBlob blob slack = peekV (blob `plusPtr` (slack + 4))
{-# INLINE peekBlob #-}
