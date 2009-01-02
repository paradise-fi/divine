{-# LANGUAGE MultiParamTypeClasses #-}
{-# LANGUAGE PatternSignatures #-}
{-# LANGUAGE FlexibleContexts #-}
{-# LANGUAGE ScopedTypeVariables #-}
{-# LANGUAGE ForeignFunctionInterface #-}

module BenchmarkHs where

import Data.Int
import Foreign.Storable
import Foreign.Ptr
import Divine.Generator

data P = P Int16
data Q = Q Int16 Int16

instance Process P where
    successors (P i) = if i > 1024 then [] else [P (i + 1)]
    initial = P 0

instance Storable P where
    sizeOf _ = sizeOf (undefined :: Int16)
    alignment _ = 4
    peek p = do x <- peek (castPtr p)
                return $ P x
    {-# INLINE peek #-}
    poke p (P x) = poke (castPtr p) x
    {-# INLINE poke #-}

instance Storable Q where
    sizeOf _ = 2 * sizeOf (undefined :: Int16)
    alignment _ = alignment (undefined :: Int16)
    peek p = do
	a <- peekElemOff (castPtr p) 0
	b <- peekElemOff (castPtr p) 1
	return (Q a b)
    {-# INLINE peek #-}
    poke p (Q a b) = do
	pokeElemOff (castPtr p) 0 a
	pokeElemOff (castPtr p) 1 b

instance Process Q where
    initial = Q 0 0
    successors (Q a b) | a < 1024 && b < 1024 =
                           [Q (a + 1) b, Q a (b + 1)]
                       | otherwise = []
    {-# INLINE successors #-}

type My = Q -- PComp P P

get_state_size = ffi_getStateSize (initial :: My)
get_initial_state t = ffi_initialState ((castPtr t) :: Ptr My)
get_successor h f t = ffi_getSuccessor h ((castPtr f) :: Ptr My) (castPtr t)
get_many_successors p g f t = ffi_getManySuccessors (initial :: My)
                                (castPtr p) (castPtr g) (castPtr f) (castPtr t)
foreign export ccall
    get_state_size :: IO CSize
foreign export ccall
    get_initial_state :: CString -> IO ()
foreign export ccall
    get_successor :: CInt -> CString -> CString -> IO CInt
foreign export ccall
    get_many_successors :: CString -> CString -> CString -> CString -> IO ()
