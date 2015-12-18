{-# LANGUAGE TemplateHaskell #-}
{-# LANGUAGE ForeignFunctionInterface #-}

module BenchmarkHs where

import Data.Int
import Data.Storable
import Foreign.Ptr
import Divine.Generator
import Divine.Blob

-- import Data.DeriveTH
-- import qualified Data.Derive.Storable

data P = P Int16
data Q = Q Int16 Int16 deriving Show

instance StorableM Q where
    sizeOfM (Q a b) = do sizeOfM a
                         sizeOfM b
    {-# INLINE sizeOfM #-}
    alignmentM ~(Q a b) = do alignmentM a
                             alignmentM b
    {-# INLINE alignmentM #-}
    peekM = do a <- peekM
               b <- peekM
               return $ Q a b
    {-# INLINE peekM #-}
    pokeM (Q a b) = do pokeM a
                       pokeM b
    {-# INLINE pokeM #-}

-- $( derive Data.Derive.Storable.makeStorable ''P )
-- $( derive Data.Derive.Storable.makeStorable ''Q )

{- TODO
{-# INLINE sizeOf #-}
{-# INLINE peek #-}
{-# INLINE poke #-}
-}

instance Process P where
    successors (P i) = if i > 128 then [] else [P (i + 1)]
    initial = P 0

instance Process Q where
    initial = Q 0 0
    successors (Q a b) | a < 128 && b < 128 =
                           [Q (a + 1) b, Q a (b + 1)]
                       | otherwise = []
    {-# INLINE successors #-}

type My = Q -- PComp P P

get_initial = ffi_initialState (undefined :: My)
get_successor = ffi_getSuccessor (undefined :: My)

foreign export ccall
    get_initial :: Ptr Setup -> Ptr Blob -> IO ()
foreign export ccall
    get_successor :: Ptr Setup -> CInt -> Blob -> Ptr Blob -> IO CInt
