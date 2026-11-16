import { Routes, Route } from 'react-router-dom';
import Layout, { ScrollToTop } from './components/Layout';
import Home from './pages/Home';
import System from './pages/System';
import Living from './pages/Living';
import Science from './pages/Science';
import { Library, DocumentView } from './pages/Library';
import Studio from './pages/Studio';
import { Journal, JournalPost } from './pages/Journal';
import { About, Interest, Privacy, NotFound } from './pages/Info';

export default function App() {
 return <Layout><ScrollToTop/><Routes>
  <Route path="/" element={<Home/>}/>
  <Route path="/system" element={<System/>}/>
  <Route path="/living" element={<Living/>}/>
  <Route path="/science" element={<Science/>}/>
  <Route path="/docs" element={<Library/>}/>
  <Route path="/docs/:id" element={<DocumentView/>}/>
  <Route path="/studio" element={<Studio/>}/>
  <Route path="/journal" element={<Journal/>}/>
  <Route path="/journal/:slug" element={<JournalPost/>}/>
  <Route path="/about" element={<About/>}/>
  <Route path="/interest" element={<Interest/>}/>
  <Route path="/privacy" element={<Privacy/>}/>
  <Route path="*" element={<NotFound/>}/>
 </Routes></Layout>;
}
