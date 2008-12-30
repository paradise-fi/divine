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
    ffi_getStateSize
    ) where

import Prelude hiding ( last, drop )
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
    , getSuccessor :: state -> [state]
    }

-- | User-level System constructor
mkSystem ::
    state ->
    (state -> [state]) ->
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
                             first :: CInt, _items :: Ptr a } deriving Show
type Blob = Ptr ()

space c = size c - count c

makeBlob :: forall x. (Storable x) => x -> IO Blob
makeBlob a = do m <- mallocBytes $ size + header
                poke (castPtr m) (fromIntegral size :: CShort)
                pokeBlob m a
                return m
    where size = sizeOf (undefined :: x)
          header = sizeOf (undefined :: CShort)

pokeBlob :: (Storable y) => Blob -> y -> IO ()
pokeBlob blob v = pokeByteOff blob 2 v

peekBlob :: (Storable y) => Blob -> IO y
peekBlob blob = peekByteOff blob 2

nth :: forall a. (Storable a) => Circular a -> Int -> Ptr a
nth c i = plusPtr (_items c) (((f + i) `mod` sz) * one)
    where one = sizeOf (undefined :: a)
          sz = fromIntegral $ size c
          f = fromIntegral $ first c

last :: forall a. (Storable a) => Circular a -> Ptr a
last c = nth c $ fromIntegral $ count c - 1

chCount :: (CInt -> CInt) -> Circular a -> Circular a
chCount f x = x { count = f $ count x }

add :: (Storable a) => a -> Ptr (Circular a) -> IO ()
add a c = do x <- chCount (+1) `fmap` peek c
             poke c x
             -- putStrLn $ "poking into slot " ++ show (last x)
             poke (last x) a

drop :: (Storable a) => Ptr (Circular a) -> IO ()
drop c = do x <- peek c
            poke c $ x { count = count x - 1,
                         first = (first x + 1) `mod` size x }

instance (Storable a) => (Storable (Circular a)) where
    peek p = do s <- peekElemOff (castPtr p) 0
                c <- peekElemOff (castPtr p) 1
                f <- peekElemOff (castPtr p) 2
                let i = plusPtr p $ 3 * sizeOf (undefined :: CInt)
                return Circular { size = s, count = c, first = f, _items = i }
    poke p c = do pokeElemOff (castPtr p) 0 (size c)
                  pokeElemOff (castPtr p) 1 (count c)
                  pokeElemOff (castPtr p) 2 (first c)
                  return ()
    sizeOf _ = 3 * sizeOf (undefined :: CInt) + sizeOf (undefined :: Ptr ())

item :: forall a. (Storable a) => Circular a -> Int -> Ptr a
item c i = plusPtr (_items c) (i * sizeOf (undefined :: a))

peekNth :: (Storable a) => Circular a -> Int -> IO a
peekNth c i = peekElemOff (_items c) i

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
  let (res, i) = (numerate' $ getSuccessor system) from' (fromIntegral handle)
  unless (i == 0) $ poke to res
  return $ fromIntegral i
{-# INLINE ffi_getSuccessor #-}

ffi_getManySuccessors :: forall state trans. (Storable state) =>
                         (System state trans) ->
                         Ptr (Circular Blob) -> Ptr (Circular Blob) -> IO ()
ffi_getManySuccessors system from to = do
  -- peek from >>= print
  -- peek to >>= print
  -- peek from >>= print
  gen
  -- peek to >>= print
  return ()
  where gen' q = do 
          from' :: Blob <- peekNth q (fromIntegral $ first q)
          fromSt <- peekBlob from'
          drop from
          sequence [ do b <- makeBlob x
                        -- putStrLn $ "adding " ++ show b
                        add from' to
                        add b to
                     | x <- getSuccessor system fromSt ]
          gen
        gen = do
          fromQ :: Circular Blob <- peek from
          toQ :: Circular Blob <- peek to
          -- FIXME hardcoded 2
          if count fromQ > 0 && space toQ >= 2 then gen' fromQ else return ()

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

