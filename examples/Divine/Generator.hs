{-# LANGUAGE MultiParamTypeClasses #-}
{-# LANGUAGE PatternSignatures #-}
{-# LANGUAGE ScopedTypeVariables #-}

module Divine.Generator (
    System(..),
    mkSystem,

    Ptr,
    CString,
    CSize,
    CInt,
    ffi_initialState,
    ffi_getSuccessor,
    ffi_getManySuccessors,
    ffi_getStateSize,
    numerate',
    numerate
    ) where

import qualified Data.ByteString as B
import qualified Data.ByteString.Unsafe as BU
import qualified Data.ByteString.Lazy as L
import qualified Data.ByteString.Lazy.Char8 as C
import Data.Binary
import Data.IORef
import Foreign.Ptr
import Foreign.C.Types
import Foreign.C.String
import Foreign.Marshal.Alloc
import Foreign.Marshal.Utils
import Foreign.Storable
import Foreign.StablePtr
import Control.Monad
import Data.Array

-- | The system datatype.
data System state trans =
    System
    { initialState :: state
    , getSuccessor :: state -> Int -> (state, Int)
    }

-- | User-level System constructor
mkSystem ::
    state ->
    (state -> Int -> (state, Int)) ->
    System state trans
mkSystem initialState getSuccessor =
    System
    { initialState = initialState
    , getSuccessor = getSuccessor
    }

--
-- FFI interface
--

data Circular a = Circular { size :: CInt, count :: CInt,
                             first :: CInt, _items :: Ptr a }
type Blob = Ptr ()

instance (Storable a) => (Storable (Circular a)) where
    peek p = do s <- peekElemOff (castPtr p) 0
                c <- peekElemOff (castPtr p) 1
                f <- peekElemOff (castPtr p) 2
                let i = plusPtr p $ 3 * sizeOf (undefined :: CInt)
                return Circular { size = s, count = c, first = f, _items = i }

item :: forall a. (Storable a) => Circular a -> Int -> Ptr a
item c i = plusPtr (_items c) (i * sizeOf (undefined :: a))

peekNth :: (Storable a) => Circular a -> Int -> IO a
peekNth c i = peekElemOff (_items c) i

pokeBlob :: (Storable y) => Blob -> y -> IO ()
pokeBlob blob v = pokeByteOff blob 2 v

-- ffi_getManySuccessors system from to =

ffi_getStateSize :: forall state trans. (Storable state) => System state trans -> IO CSize
ffi_getStateSize _ = return $ fromIntegral $ sizeOf (undefined :: state)

ffi_initialState :: (Show state, Storable state) => System state trans -> CString -> IO ()
ffi_initialState system out = do
  let ini = initialState system
      out' = (castPtr :: CString -> Ptr state) out
  poke out' ini
  return ()
{-# INLINE ffi_initialState #-}

ffi_getSuccessor :: forall state trans. (Show state, Storable state)
                    => System state trans ->
                        CInt -> Ptr state -> Ptr state -> IO CInt
ffi_getSuccessor system handle from to = do
  from' <- peek from
  let (res, i) = getSuccessor system from' (fromIntegral handle)
  unless (i == 0) $ poke to res
  return $ fromIntegral i
{-# INLINE ffi_getSuccessor #-}

ffi_getManySuccessors :: (System state trans) ->
                         Ptr (Circular Blob) -> Ptr (Circular Blob) -> IO ()
ffi_getManySuccessors system from to = do
  f <- peek from
  t <- peek to
  print (size f, count f, first f)
  print (size t, count t, first t)

instance Storable () where
    sizeOf _ = 0
    alignment _ = 1
    peek p = return ()
    poke p _ = return ()

numerate :: (x -> [x]) -> x -> Int -> (x, Int)
numerate f x i = (arr ! i, if i == max then 0 else i + 1)
    where arr = listArray (1, max) succs
          max = 1 + length succs
          succs = f x

numerate' :: (x -> [x]) -> x -> Int -> (x, Int)
numerate' f x i = (succs !! (i - 1), if i == max then 0 else i + 1)
    where max = 1 + length succs
          succs = f x

