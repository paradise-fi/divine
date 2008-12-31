{-# LANGUAGE ForeignFunctionInterface #-}
module Divine.Pool ( Pool, Group, get, alloc, ccall_alloc ) where

import Prelude hiding ( last )
import Foreign.C.Types
import Foreign.Ptr
import Foreign.Storable
import Control.Monad( when )

foreign import ccall "divine/pool.ffi.h pool_extend"
        pool_extend :: Ptr Pool -> CSize -> IO (Ptr Group)

foreign import ccall "divine/pool.ffi.h pool_allocate"
        pool_allocate :: Ptr Pool -> CSize -> IO (Ptr a)

data Group = Group { -- item :: !CSize, used :: !CSize, total :: !CSize,
                     free :: !(Ptr (Ptr ())),
                     current :: !(Ptr ()),
                     last :: !(Ptr ()) }

data Pool = Pool { ptr :: !(Ptr Pool),
                   groupCount :: !CInt,
                   groups :: !(Ptr Group) } deriving Show

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

get :: Ptr Pool -> Ptr Group -> IO Pool
get pool grp = do
  x <- peek (castPtr pool)
  return $ Pool { ptr = pool, groupCount = x, groups = castPtr grp }
{-# INLINE get #-}

group :: Pool -> Int -> IO Group
group p sz = peekElemOff (groups p) (sz `div` 4)

pokeGroup :: Pool -> Group -> Int -> IO ()
pokeGroup p g sz = pokeElemOff (groups p) (sz `div` 4) g
{-# INLINE  pokeGroup #-}

ensureFreespace :: Pool -> Int -> IO Group
ensureFreespace p sz = do
  if (groupCount p <= (fromIntegral sz) `div` 4)
      then do grp <- pool_extend (ptr p) csz
              p <- get (ptr p) grp
              grpExtend p
      else grpExtend p
    where grpExtend p =
              do g <- group p sz
                 when (free g == nullPtr && current g `plusPtr` sz > last g) $
                      pool_extend (ptr p) csz >> return ()
                 group p sz >>= return
          csz = fromIntegral sz
{-# INLINE ensureFreespace #-}

alloc :: Pool -> Int -> IO (Ptr a)
alloc p sz' = do
  let sz = fromIntegral $ ptrToIntPtr $
             alignPtr (intPtrToPtr (fromIntegral sz')) 4
  -- putStrLn $ "allocating " ++ show sz ++ " bytes in pool " ++ show p
  g <- ensureFreespace p (fromIntegral sz)
  (nf, nc) <- if (free g == nullPtr)
                then return (nullPtr, current g `plusPtr` sz)
                else do x <- peek (castPtr $ free g)
                        return (x, current g)
  let g' = g { current = nc, free = castPtr nf }
  pokeGroup p g' sz
  return $ castPtr (if nf == nullPtr then current g else nf)
{-# INLINE alloc #-}

ccall_alloc p sz = pool_allocate (ptr p) (fromIntegral sz)
{-# INLINE ccall_alloc #-}
