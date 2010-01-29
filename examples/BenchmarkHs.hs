{-# LANGUAGE TemplateHaskell #-}
{-# LANGUAGE ForeignFunctionInterface #-}

module BenchmarkHs where

import Data.Int
import Data.Storable
import Foreign.Ptr
import Divine.Generator

-- import Data.DeriveTH
-- import qualified Data.Derive.Storable

data P = P Int16
data Q = Q Int16 Int16

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

get_state_size = ffi_getStateSize (initial :: My)
get_initial_state t = ffi_initialState ((castPtr t) :: Ptr My)
get_successor h f t = ffi_getSuccessor h ((castPtr f) :: Ptr My) (castPtr t)
get_many_successors sl p g f t = ffi_getManySuccessors (initial :: My)
                                   (fromIntegral sl)
                                   (castPtr p) (castPtr g) (castPtr f) (castPtr t)
foreign export ccall
    get_state_size :: IO CSize
foreign export ccall
    get_initial_state :: CString -> IO ()
foreign export ccall
    get_successor :: CInt -> CString -> CString -> IO CInt
foreign export ccall
    get_many_successors :: CInt -> CString -> CString -> CString -> CString -> IO ()
