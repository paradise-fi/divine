{-# LANGUAGE ForeignFunctionInterface #-}
module Divine.Pool ( Pool, Group, get, alloc, ccall_alloc ) where

import Prelude hiding ( last )
import Foreign.C.Types
import Foreign.Ptr
import Foreign.Storable
import Control.Monad( when )

foreign import ccall "pool_extend"
        pool_extend :: Ptr () -> CInt -> IO (Ptr Group)

foreign import ccall "pool_allocate"
        pool_allocate :: Ptr () -> CInt -> IO (Ptr a)

data Group = Group { -- item :: !CSize, used :: !CSize, total :: !CSize,
                     free :: !(Ptr (Ptr ())),
                     current :: !(Ptr ()),
                     last :: !(Ptr ()) }

type Pool = (Ptr (), Ptr Group)

sz_ptr = sizeOf (undefined :: Ptr ())
sz_size = sizeOf (undefined :: CSize)

instance Storable Group where
    sizeOf _ = 3 * sizeOf (undefined :: CSize) +
                     2 * 3 * sizeOf (undefined :: Ptr ())
    peek p = do f <- peekByteOff (castPtr p) (3 * sz_size)
                c <- peekByteOff (castPtr p) (3 * sz_size + sz_ptr)
                l <- peekByteOff (castPtr p) (3 * sz_size + 2 * sz_ptr)
                return $ Group { free = f, current = c, last = l }
    poke p g = do pokeByteOff (castPtr p) (3 * sz_size) (free g)
                  pokeByteOff (castPtr p) (3 * sz_size + sz_ptr) (current g)
                  pokeByteOff (castPtr p) (3 * sz_size + 2 * sz_ptr) (last g)
                  return ()

pokeCurrent :: Ptr Group -> Ptr () -> IO ()
pokeCurrent p cur = pokeByteOff (castPtr p) (3 * sz_size + sz_ptr) cur
{-# INLINE pokeCurrent #-}

pokeFree :: Ptr Group -> Ptr (Ptr ()) -> IO ()
pokeFree p fr = pokeByteOff (castPtr p) (3 * sz_size) fr

get :: Ptr () -> Ptr Group -> IO Pool
get pool grp = return (pool, grp)
{-# INLINE get #-}

groups = snd
ptr = fst
groupCount :: Pool -> IO CInt
groupCount p = peek (castPtr $ ptr p)

group :: Pool -> Int -> IO Group
group p sz = peekElemOff (groups p) (sz `div` 4)

groupPtr :: Pool -> Int -> IO (Ptr Group)
groupPtr p sz = return $ groups p `plusPtr`
                  (sz * (sizeOf (undefined :: Group) `div` 4))
{-# INLINE  groupPtr #-}

pokeGroup :: Pool -> Group -> Int -> IO ()
pokeGroup p g sz = pokeElemOff (groups p) (sz `div` 4) g
{-# INLINE  pokeGroup #-}

ensureFreespace :: Pool -> Ptr Group -> Int -> IO Group
ensureFreespace p gptr sz = do
  cnt <- groupCount p
  let csz = fromIntegral sz
  if (4 * cnt <= csz)
      then do pool_extend (ptr p) csz
              peek gptr
      else do g <- peek gptr
              if (free g == nullPtr && current g `plusPtr` sz > last g)
                 then pool_extend (ptr p) csz >> peek gptr
                 else return g
{-# INLINE ensureFreespace #-}

alloc :: Pool -> Int -> IO (Ptr a)
alloc p sz' = do
  let sz = 4 * ((3 + sz') `div` 4) -- align to 4
  gptr <- groupPtr p sz
  g <- ensureFreespace p gptr sz
  if (free g == nullPtr)
    then do pokeCurrent gptr (current g `plusPtr` sz)
            return $ castPtr $ current g
    else do x <- peek (castPtr $ free g) -- dereference
            pokeFree gptr x
            return $ castPtr $ free g
{-# INLINE alloc #-}

ccall_alloc p sz = pool_allocate (ptr p) (fromIntegral sz)
{-# INLINE ccall_alloc #-}
