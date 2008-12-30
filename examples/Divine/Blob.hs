{-# LANGUAGE ScopedTypeVariables #-}
module Divine.Blob ( Blob, makeBlob, pokeBlob, peekBlob ) where

import Foreign.Storable
import Foreign.Ptr
import Foreign.C.Types
import Foreign.Marshal.Alloc

type Blob = Ptr ()

makeBlob :: forall x. (Storable x) => x -> IO Blob
makeBlob a = do m <- mallocBytes $ size + header
                poke (castPtr m) (fromIntegral size :: CShort)
                pokeBlob m a
                return m
    where size = sizeOf (undefined :: x)
          header = sizeOf (undefined :: CShort)
{-# INLINE makeBlob #-}

pokeBlob :: (Storable y) => Blob -> y -> IO ()
pokeBlob blob v = pokeByteOff blob 2 v

peekBlob :: (Storable y) => Blob -> IO y
peekBlob blob = peekByteOff blob 2

