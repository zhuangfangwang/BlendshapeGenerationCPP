#include "blendshapegeneration.h"
#include <QFileDialog>

BlendshapeVisualizer::BlendshapeVisualizer(QWidget *parent):
  GL3DCanvas(parent)
{
  setSceneScale(2.0);
  mouseInteractionMode = GL3DCanvas::VIEW_TRANSFORM;
}

BlendshapeVisualizer::~BlendshapeVisualizer()
{

}

void BlendshapeVisualizer::loadMesh(const string &filename)
{
  mesh.load(filename);
  mesh.computeNormals();
  computeDistance();
  repaint();
}

void BlendshapeVisualizer::loadReferenceMesh(const string &filename)
{
  refmesh.load(filename);
  computeDistance();
  repaint();
}

void BlendshapeVisualizer::paintGL()
{
  GL3DCanvas::paintGL();

  glEnable(GL_DEPTH_TEST);

  enableLighting();
  if( mesh.faces.nrow > 0 ) {
    if( refmesh.faces.nrow > 0 ) drawMeshWithColor(mesh);
    else drawMesh(mesh);
  }
  disableLighting();

  if( !dists.empty() ) {
    double minVal = (*std::min_element(dists.begin(), dists.end()));
    double maxVal = (*std::max_element(dists.begin(), dists.end()));
    string minStr = "min: " + to_string(minVal);
    string maxStr = "max: " + to_string(maxVal);
    glColor4f(0, 0, 1, 1);
    renderText(25, 25, QString(minStr.c_str()));
    glColor4f(1, 0, 0, 1);
    renderText(25, 45, QString(maxStr.c_str()));

    drawColorBar(minVal, maxVal);
  }
}

void BlendshapeVisualizer::drawColorBar(double minVal, double maxVal) {
  // setup 2D view
  glViewport(0, 0, width(), height());
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, width(), height(), 0, -1, 1);

  int nsegments = 128;

  double left = 0.85 * width();
  double w = 0.025 * width();
  double top = 0.25 * height();
  double h = 0.5 * height() / (nsegments-1);

  vector<QColor> colors(nsegments);
  for(int i=0;i<nsegments;++i) {
    double c = i / float(nsegments-1);
    double hval0 = c;
    colors[i] = QColor::fromHsvF(hval0*0.67, 1.0, 1.0);
  }

  for(int i=0;i<nsegments-1;++i) {
    glBegin(GL_QUADS);
    glNormal3f(0, 0, -1);

    glColor4f(colors[i].redF(), colors[i].greenF(), colors[i].blueF(), 1.0);
    float hstart = i * h;
    float hend = hstart + h;
    glVertex3f(left, top+hstart, -0.5); glVertex3f(left + w, top+hstart, -0.5);

    glColor4f(colors[i+1].redF(), colors[i+1].greenF(), colors[i+1].blueF(), 1.0);
    glVertex3f(left + w, top + hend, -0.5); glVertex3f(left, top + hend, -0.5);

    glEnd();
  }

  // draw texts
  glColor4f(0, 0, 0, 1);
  int ntexts = 6;
  for(int i=0;i<ntexts;++i) {
    double ratio = 1.0 - i / (float)(ntexts-1);
    double hpos = i * (0.5 * height()) / (ntexts - 1);
    string str = to_string(minVal + (maxVal - minVal) * ratio);
    renderText(left + w, top+hpos + 5.0, -0.5, QString(str.c_str()));
  }
}

void BlendshapeVisualizer::enableLighting()
{
  GLfloat light_position[] = {10.0, 4.0, 10.0,1.0};
  GLfloat mat_specular[] = {0.5, 0.5, 0.5, 1.0};
  GLfloat mat_diffuse[] = {0.375, 0.375, 0.375, 1.0};
  GLfloat mat_shininess[] = {25.0};
  GLfloat light_ambient[] = {0.05, 0.05, 0.05, 1.0};
  GLfloat white_light[] = {1.0, 1.0, 1.0, 1.0};

  glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mat_specular);
  glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, mat_shininess);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, mat_diffuse);

  glLightfv(GL_LIGHT0, GL_POSITION, light_position);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, white_light);
  glLightfv(GL_LIGHT0, GL_SPECULAR, white_light);
  glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);

  light_position[0] = -10.0;
  glLightfv(GL_LIGHT1, GL_POSITION, light_position);
  glLightfv(GL_LIGHT1, GL_DIFFUSE, white_light);
  glLightfv(GL_LIGHT1, GL_SPECULAR, white_light);
  glLightfv(GL_LIGHT1, GL_AMBIENT, light_ambient);

  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);
  glEnable(GL_LIGHT1);
}

void BlendshapeVisualizer::disableLighting()
{
  glDisable(GL_LIGHT0);
  glDisable(GL_LIGHT1);
  glDisable(GL_LIGHTING);
}

void BlendshapeVisualizer::computeDistance()
{
  if( refmesh.faces.nrow <= 0 ) return;

  int nfaces = mesh.faces.nrow;

  std::vector<Triangle> triangles;
  triangles.reserve(nfaces);
  for(int i=0,ioffset=0;i<nfaces;++i) {
    int v1 = refmesh.faces(ioffset++), v2 = refmesh.faces(ioffset++), v3 = refmesh.faces(ioffset++);
    auto p1 = refmesh.verts.rowptr(v1), p2 = refmesh.verts.rowptr(v2), p3 = refmesh.verts.rowptr(v3);
    Point a(p1[0], p1[1], p1[2]);
    Point b(p2[0], p2[1], p2[2]);
    Point c(p3[0], p3[1], p3[2]);

    triangles.push_back(Triangle(a, b, c));
  }

  Tree tree(triangles.begin(), triangles.end());

  dists.resize(mesh.verts.nrow);
  for(int i=0;i<mesh.verts.nrow;++i) {
    double px = mesh.verts(i, 0),
           py = mesh.verts(i, 1),
           pz = mesh.verts(i, 2);
    Tree::Point_and_primitive_id bestHit = tree.closest_point_and_primitive(Point(px, py, pz));
    double qx = bestHit.first.x(),
           qy = bestHit.first.y(),
           qz = bestHit.first.z();

    double dx = px - qx, dy = py - qy, dz = pz - qz;
    dists[i] = sqrt(dx*dx+dy*dy+dz*dz);
  }
}

void BlendshapeVisualizer::drawMesh(const BasicMesh &m)
{
  glColor4f(0.75, 0.75, 0.75, 1.0);
  glBegin(GL_TRIANGLES);
  for(int i=0;i<m.faces.nrow;++i) {
    int *v = m.faces.rowptr(i);
    glNormal3dv(m.norms.rowptr(i));
    glVertex3dv(m.verts.rowptr(v[0]));
    glVertex3dv(m.verts.rowptr(v[1]));
    glVertex3dv(m.verts.rowptr(v[2]));
  }
  glEnd();
}

void BlendshapeVisualizer::drawMeshWithColor(const BasicMesh &m)
{
  double maxVal = *(std::max_element(dists.begin(), dists.end()));
  double minVal = *(std::min_element(dists.begin(), dists.end()));

  glColor4f(0.75, 0.75, 0.75, 1.0);
  glBegin(GL_TRIANGLES);
  for(int i=0;i<m.faces.nrow;++i) {
    int *v = m.faces.rowptr(i);
    glNormal3dv(m.norms.rowptr(i));

    double hval0 = 1.0 - max(min((dists[v[0]]-minVal)/(maxVal-minVal)/0.67, 1.0), 0.0);
    QColor c0 = QColor::fromHsvF(hval0*0.67, 1.0, 1.0);
    float colors0[4] = {c0.redF(), c0.greenF(), c0.blueF(), 1.0};
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, colors0);
    glVertex3dv(m.verts.rowptr(v[0]));

    double hval1 = 1.0 - max(min((dists[v[1]]-minVal)/(maxVal-minVal)/0.67, 1.0), 0.0);
    QColor c1 = QColor::fromHsvF(hval1*0.67, 1.0, 1.0);
    float colors1[4] = {c1.redF(), c1.greenF(), c1.blueF(), 1.0};
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, colors1);
    glVertex3dv(m.verts.rowptr(v[1]));

    double hval2 = 1.0 - max(min((dists[v[2]]-minVal)/(maxVal-minVal)/0.67, 1.0), 0.0);
    QColor c2 = QColor::fromHsvF(hval2*0.67, 1.0, 1.0);
    float colors2[4] = {c2.redF(), c2.greenF(), c2.blueF(), 1.0};
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, colors2);
    glVertex3dv(m.verts.rowptr(v[2]));
  }
  glEnd();
}

BlendshapeGeneration::BlendshapeGeneration(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);

    canvas = new BlendshapeVisualizer(this);
    setCentralWidget(canvas);

    connect(ui.actionLoad_Mesh, SIGNAL(triggered()), this, SLOT(slot_loadMesh()));
    connect(ui.actionLoad_Reference, SIGNAL(triggered()), this, SLOT(slot_loadReferenceMesh()));
}

BlendshapeGeneration::~BlendshapeGeneration()
{

}

void BlendshapeGeneration::slot_loadMesh()
{
  QString filename = QFileDialog::getOpenFileName();
  canvas->loadMesh(filename.toStdString());
}

void BlendshapeGeneration::slot_loadReferenceMesh()
{
  QString filename = QFileDialog::getOpenFileName();
  canvas->loadReferenceMesh(filename.toStdString());
}

