/***************************************************************************
 *   (C) 2005-2006 Marius Roets <roets.marius@gmail.com>                   *
 *   (C) 2005-2009 Michal Rudolf <mrudolf@kdewebdev.org>                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include <QtDebug>
#include <QFile>
#include "annotation.h"
#include "ecopositions.h"
#include "gamex.h"
#include "tags.h"

using namespace chessx;

#if defined(_MSC_VER) && defined(_DEBUG)
#define DEBUG_NEW new( _NORMAL_BLOCK, __FILE__, __LINE__ )
#define new DEBUG_NEW
#endif // _MSC_VER


MoveTree::MoveTree()
    : m_currentBoard(nullptr)
    , mountRefCount(0)
    , m_nodes()
    , m_currentNode(0)
    , m_startPly(0)
    , m_startingBoard()
{
    mountBoard();
}

MoveTree::MoveTree(const MoveTree& rhs)
    : m_currentBoard(nullptr)
    , mountRefCount(0)
    , m_nodes(rhs.m_nodes)
    , m_currentNode(rhs.m_currentNode)
    , m_startPly(rhs.m_startPly)
    , m_startingBoard(rhs.m_startingBoard)
{
    mountBoard();
}

MoveTree& MoveTree::operator=(const MoveTree& rhs)
{
    m_nodes = rhs.m_nodes;
    m_currentNode = rhs.m_currentNode;
    m_startPly = rhs.m_startPly;
    m_startingBoard = rhs.m_startingBoard;
    if (m_currentBoard && rhs.m_currentBoard)
    {
        *m_currentBoard = *rhs.m_currentBoard;
    }
    return *this;
}

MoveTree::~MoveTree()
{
    if (mountRefCount)
    {
        delete m_currentBoard;
        m_currentBoard = nullptr;
    }
}

void MoveTree::mountBoard()
{
    ++mountRefCount;
    if (mountRefCount == 1)
    {
        m_currentBoard = new BoardX;
    }
}

void MoveTree::unmountBoard()
{
    if (mountRefCount > 0)
    {
        --mountRefCount;
        if (mountRefCount == 0)
        {
            delete m_currentBoard;
            m_currentBoard = nullptr;
        }
    }
}

MoveId MoveTree::makeNodeIndex(MoveId moveId) const
{
    if (moveId == CURRENT_MOVE)
    {
        moveId = m_currentNode;
    }
    bool rangeOk = 0 <= moveId && moveId < m_nodes.size();
    if (!rangeOk || m_nodes[moveId].Removed())
    {
        return NO_MOVE;
    }
    return moveId;
}

static const char strSquareNames[64][3] =
{
    "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1",
    "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2",
    "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3",
    "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4",
    "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5",
    "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6",
    "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7",
    "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8"
};

const QStringList GameX::s_specList = QStringList() << s_emt << s_clk << s_egt << s_csl << s_cal;

GameX::GameX()
    : QObject()
    , m_moves()
{
    clearTags();
    clear();
}

GameX::GameX(const GameX& game)
    : QObject()
    , m_moves(game.m_moves)
{
    *this = game;
}

GameX& GameX::operator=(const GameX& game)
{
    if(this != &game)
    {
        clearTags();
        clear();
        //assign non pointer variables
        m_tags = game.m_tags;
        //m_tags.detach();
        m_variationStartAnnotations = game.m_variationStartAnnotations;
        //m_variationStartAnnotations.detach();
        m_annotations = game.m_annotations;
        //m_annotations.detach();
        //m_moveNodes.detach();
        m_moves = game.m_moves;
        if (m_moves.currentBoard() && !game.m_moves.currentBoard())
        {
            moveToStart();
        }
    }
    return *this;
}

GameX::~GameX()
{
}

void GameX::copyFromGame(const GameX& g)
{
    *this = g;
    emit signalGameModified(true,*this,tr("Copy game"));
}

MoveId GameX::dbAddMove(const Move& move, const QString& annotation, NagSet nags)
{
    MoveNode node;
    MoveId previousNode = m_moves.m_currentNode;

    node.nextNode = NO_MOVE;
    node.previousNode = m_moves.m_currentNode;
    node.parentNode = m_moves.m_nodes[m_moves.m_currentNode].parentNode;
    node.move = move;
    node.nags = nags;
    node.SetPly(ply() + 1);
    m_moves.m_nodes.append(node);
    m_moves.m_currentNode = m_moves.m_nodes.size() - 1;
    m_moves.m_nodes[previousNode].nextNode = m_moves.m_currentNode;
    if(!annotation.isEmpty())
    {
        dbSetAnnotation(annotation, m_moves.m_currentNode);
    }
    m_moves.currentBoard()->doMove(move);
    return m_moves.m_currentNode;
}

MoveId GameX::addMove(const Move& move, const QString& annotation, NagSet nags)
{
    GameX state = *this;
    dbAddMove(move,annotation,nags);
    dbIndicateAnnotationsOnBoard(m_moves.m_currentNode);
    emit signalGameModified(true,state,tr("Add move"));
    return m_moves.m_currentNode;
}

MoveId GameX::dbAddSanMove(const QString& sanMove, const QString& annotation, NagSet nags)
{
    Move move = m_moves.currentBoard()->parseMove(sanMove);
    if(move.isLegal() || move.isNullMove())
    {
        return dbAddMove(move, annotation, nags);
    }
    // qDebug() << sanMove << " : " << move.toAlgebraicDebug() << " is illegal in position " << board().toHumanFen() << " / " << board().toFen();
    return NO_MOVE;
}


MoveId GameX::addMove(const QString& sanMove, const QString& annotation, NagSet nags)
{
    Move move = m_moves.currentBoard()->parseMove(sanMove);
    if(move.isLegal() || move.isNullMove())
    {
        return addMove(move, annotation, nags);
    }
    return NO_MOVE;
}

MoveId GameX::addMoveFrom64Char(const QString &qcharboard)
{
    QStringList l = qcharboard.split(' ');
    if (l.size() < 30) return NO_MOVE;
    Char64Relation relation = (Char64Relation) l[C64_GAME_RELATION].toInt();
    QString s=l[C64_PP_LAST_MOVE];
    s.remove('+');
    s.remove('#');

    QString t = l[C64_ELAPSED_TIME_LAST_MOVE];
    t.remove("(");
    t.remove(")");
    QStringList tl = t.split(':');
    QString emt;
    if (tl.size()>=2)
    {
        emt = QString("[%emt 0:%1:%2]").arg(tl[0],-2,'0').arg(tl[1],-2,'0');
    }

    if (relation == C64_REL_PLAY_OPPONENT_MOVE)
    {
        if (s=="none")
        {
            return NO_MOVE;
        }
        else
        {
            if (!emt.isEmpty())
            {
                setAnnotation(emt);
            }
            return CURRENT_MOVE;
        }
    }
    else
    {
        Color thisMoveColor = (l[C64_COLOR_TO_MOVE]=="W") ? Black : White;
        if (thisMoveColor == board().toMove())
        {
            MoveId moveId = addMove(s,emt);
            return moveId;
        }
        else
        {
            return NO_MOVE;
        }
    }
}

bool GameX::mergeNode(GameX& otherGame)
{
    SaveRestoreMove saveThis(*this);
    SaveRestoreMove saveOther(otherGame);

    QString ann;
    NagSet nags;
    otherGame.forward();
    QString san = otherGame.moveToSan(MoveOnly, PreviousMove, CURRENT_MOVE, &ann, &nags);
    bool retVal = (NO_MOVE != dbAddSanMove(san, ann, nags));
    return retVal;
}

bool GameX::mergeVariations(GameX& otherGame)
{
    QList<MoveId> variationList = otherGame.variations();
    bool ok = true;
    if(!variationList.empty())
    {
        MoveId otherCurrent = otherGame.currentMove();
        for(QList<MoveId>::iterator iter = variationList.begin(); iter != variationList.end(); ++iter)
        {
            otherGame.enterVariation(*iter);
            if(!mergeAsVariation(otherGame))
            {
                ok = false;
            }
            otherGame.dbMoveToId(otherCurrent);
            if(!ok)
            {
                break;
            }
        }
    }
    return ok;
}

bool GameX::mergeAsMainline(GameX& otherGame)
{
    SaveRestoreMove saveNode(*this);

    QString ann;
    NagSet nags;

    QString san = otherGame.moveToSan(MoveOnly, PreviousMove, CURRENT_MOVE, &ann, &nags);
    if(NO_MOVE != dbAddSanMove(san, ann, nags))
    {
        while(!otherGame.atLineEnd())
        {
            forward();
            if(!mergeNode(otherGame))
            {
                return false;
            }
            mergeVariations(otherGame);
            otherGame.forward();
        }
    }
    else
    {
        return false;
    }
    return true;
}

bool GameX::mergeAsVariation(GameX& otherGame)
{
    SaveRestoreMove saveNode(*this);

    QString ann;
    NagSet nags;

    QString san = otherGame.moveToSan(MoveOnly, PreviousMove, CURRENT_MOVE, &ann, &nags);
    if(NO_MOVE != dbAddSanVariation(m_moves.m_currentNode, san, ann, nags))
    {
        while(!otherGame.atLineEnd())
        {
            forward();
            if(!mergeNode(otherGame))
            {
                return false;
            }
            mergeVariations(otherGame);
            otherGame.forward();
        }
    }
    else
    {
        return false;
    }
    return true;
}

MoveId GameX::findMergePoint(const GameX& otherGame)
{
    MoveId prevNode = NO_MOVE;
    MoveId trailNode = NO_MOVE;
    MoveId otherMergeNode = NO_MOVE;
    bool found = false;
    do
    {
        if(NO_MOVE == (otherMergeNode = otherGame.findPosition(board())))
        {
            if(trailNode != NO_MOVE)
            {
                found = true;
                otherMergeNode = trailNode;
                break;
            }
        }
        else
        {
            prevNode = trailNode;
            trailNode = otherMergeNode;
        }
    }
    while(forward());

    if(!found && atLineEnd())
    {
        // Both games are identical up to the end of this game
        // Need to go one move back in the game to merge
        otherMergeNode = prevNode;
    }

    if(otherMergeNode == NO_MOVE)
    {
        if(otherGame.m_moves.m_nodes.size() > 0)
        {
            otherMergeNode = 0;
        }
    }

    // otherMergeNode points to the move before the game diverges
    // If the complete games needs to be merged, it points to node 0

    return otherMergeNode;
}

void GameX::dbMergeWithGame(const GameX& g)
{
    MoveId saveNode = m_moves.m_currentNode;
    GameX otherGame = g;
    otherGame.moveToEnd();

    // Set the game information on the last move so that it is merged into this game
    QString white = otherGame.tag(TagNameWhite);
    QString black = otherGame.tag(TagNameBlack);
    QString event = otherGame.eventInfo();
    QString shortDescription;
    if (!(white.isEmpty() && black.isEmpty() && event.isEmpty()))
    {
        if (white.isEmpty()) white = "?";
        if (black.isEmpty()) black = "?";
        shortDescription = QString("%1-%2 %3").arg(white).arg(black).arg(event);
        otherGame.dbSetAnnotation(shortDescription);
    }

    MoveId otherMergeNode = findMergePoint(otherGame);

    if(otherMergeNode != NO_MOVE)
    {
        backward();
        // merge othergame starting with otherMergeNode into variation starting from m_currentNode
        otherGame.dbMoveToId(otherMergeNode);
        otherGame.forward();
        if(atLineEnd())
        {
            mergeAsMainline(otherGame);
        }
        else
        {
            mergeAsVariation(otherGame);
        }
        otherGame.dbMoveToId(otherMergeNode);
        mergeVariations(otherGame);
    }

    // undo changes
    if(saveNode != m_moves.m_currentNode)
    {
        dbMoveToId(saveNode);
    }
    compact();
}

void GameX::mergeWithGame(const GameX& g)
{
    GameX state = *this;
    dbMergeWithGame(g);
    emit signalGameModified(true,state,tr("Merge game"));
}

bool GameX::positionRepetition(const BoardX& b)
{
    int repCount = 1;
    while(backward())
    {
        if (board() == b)
        {
            repCount++;
            if (repCount >= 3) break;
        }
    }
    return repCount >= 3;
}

bool GameX::positionRepetition3(const BoardX& b) const
{
    GameX g = *this;
    return g.positionRepetition(b);
}

bool GameX::insufficientMaterial(const BoardX& b) const
{
    return b.insufficientMaterial();
}

QString GameX::eventInfo() const
{
    QString result;
    QString site = tag(TagNameSite).left(30).remove("?");
    QString event = tag(TagNameEvent).left(30).remove("?");
    if(!site.isEmpty())
    {
        result.append(site);
        if(tag("Round") != "?")
        {
            result.append(QString(" (%1)").arg(tag("Round")));
        }
        if(!event.isEmpty())
        {
            result.append(", ");
        }
    }
    result.append(event);
    if(!tag("Date").startsWith("?"))
    {
        if(result.length() > 4)
        {
            result.append(", ");
        }
        result.append(tag(TagNameDate).remove(".??"));
    }
    return result;
}

bool GameX::currentNodeHasVariation(Square from, Square to) const
{
    if(m_moves.m_currentNode == NO_MOVE)
    {
        return false;
    }

    const QList<MoveId>& vs = currentVariations();
    QList<MoveId>::const_iterator i;
    for(i = vs.constBegin(); i != vs.constEnd(); ++i)
    {
        Move m = move(*i);
        if(m.from() == from && m.to() == to)
        {
            return true;
        }
    }
    return false;
}

const QList<MoveId>& GameX::currentVariations() const
{
    return m_moves.m_nodes[m_moves.m_currentNode].variations;
}

bool GameX::currentNodeHasMove(Square from, Square  to) const
{
    if(currentNodeHasVariation(from, to))
    {
        return true;
    }
    MoveId node = m_moves.m_nodes[m_moves.m_currentNode].nextNode;
    if(node == NO_MOVE)
    {
        return false;
    }
    Move m = m_moves.m_nodes[node].move;
    return (m.from() == from && m.to() == to);
}

bool GameX::hasNextMove() const
{
    MoveId node = m_moves.m_nodes[m_moves.m_currentNode].nextNode;
    return (node != NO_MOVE);
}

bool GameX::findNextMove(Move m)
{
    return findNextMove(m.from(),m.to(),m.isPromotion() ? pieceType(m.promotedPiece()) : None);
}

// does the next main move or one of the variations go from square from to square to
// if so make it on the board
bool GameX::findNextMove(Square from, Square to, PieceType promotionPiece)
{
    MoveId node;
    node = m_moves.m_nodes[m_moves.m_currentNode].nextNode;
    if(node != NO_MOVE)
    {
        Move m = m_moves.m_nodes[node].move ;
        if(m.from() == from && m.to() == to &&
                ((promotionPiece == None) || ((m.isPromotion() && (pieceType(m.promotedPiece()) == promotionPiece)))))
        {
            forward();
            return true;
        }
        else
        {
            QList<MoveId> vs = m_moves.m_nodes[m_moves.m_currentNode].variations;
            QList<MoveId>::iterator i;
            for(i = vs.begin(); i != vs.end(); ++i)
            {
                Move m = move(*i);
                if(m.from() == from && m.to() == to &&
                        ((promotionPiece == None) || ((m.isPromotion() && (pieceType(m.promotedPiece()) == promotionPiece)))))
                {
                    dbMoveToId(*i);
                    return true;
                }
            }
        }
    }
    return false;
}


bool GameX::replaceMove(const Move& move, const QString& annotation, NagSet nags, bool bReplace)
{
    MoveId node;
    GameX state = *this;

    node = m_moves.m_nodes[m_moves.m_currentNode].nextNode;
    if(node == NO_MOVE)
    {
        dbAddMove(move, annotation, nags);
        emit signalGameModified(true,state,tr("Add move"));
        return true;
    }

    //replace node data with new move
    m_moves.m_nodes[node].move = move;
    m_moves.m_nodes[node].nags = nags;
    dbSetAnnotation(annotation, node);

    //remove any following nodes after replaced move by disconnecting them from the tree
    forward();
    if(bReplace)
    {
        truncateVariation();
    }
    else
    {
        truncateVariationAfterNextIllegalPosition();
        compact();
    }
    backward();
    emit signalGameModified(true,state,tr("Replace move"));

    return true;
}

bool GameX::replaceMove(const QString& sanMove)
{
    return replaceMove(m_moves.currentBoard()->parseMove(sanMove), QString(), NagSet(), true);
}

bool GameX::insertMove(Move m)
{
    return replaceMove(m, annotation(), nags(), false);
}

MoveId GameX::addLine(const Move::List& moveList, const QString& annotation)
{
    GameX state = *this;
    MoveId retVal = dbAddLine(moveList, annotation);
    dbIndicateAnnotationsOnBoard(m_moves.m_currentNode);
    if (retVal != NO_MOVE)
    {
        emit signalGameModified(true, state, tr("Add line"));
    }
    return retVal;
}

MoveId GameX::addVariation(const Move& move, const QString& annotation, NagSet nags)
{
    GameX state = *this;
    MoveId retVal = dbAddVariation(move, annotation, nags);
    dbIndicateAnnotationsOnBoard(m_moves.m_currentNode);
    if (retVal != NO_MOVE)
    {
        emit signalGameModified(true, state, tr("Add variation"));
    }
    return retVal;
}

MoveId GameX::addVariation(const Move::List& moveList, const QString& annotation)
{
    GameX state = *this;
    MoveId retVal = dbAddVariation(moveList, annotation);
    dbIndicateAnnotationsOnBoard(m_moves.m_currentNode);
    if (retVal != NO_MOVE)
    {
        emit signalGameModified(true, state, tr("Add variation"));
    }
    return retVal;
}

MoveId GameX::addVariation(const QString& sanMove, const QString& annotation, NagSet nags)
{
    GameX state = *this;
    MoveId retVal = dbAddSanVariation(m_moves.m_currentNode, sanMove, annotation, nags);
    dbIndicateAnnotationsOnBoard(m_moves.m_currentNode);
    if (retVal != NO_MOVE)
    {
        emit signalGameModified(true, state, tr("Add variation"));
    }
    return retVal;
}

MoveId GameX::dbAddVariation(const Move& move, const QString& annotation, NagSet nags)
{
    MoveId previousNode = m_moves.m_currentNode;
    MoveId saveNextNode = m_moves.m_nodes[m_moves.m_currentNode].nextNode;
    MoveId node = dbAddMove(move, annotation, nags);
    m_moves.m_nodes[m_moves.m_currentNode].parentNode = previousNode;
    m_moves.m_nodes[previousNode].variations.append(node);
    m_moves.m_nodes[previousNode].nextNode = saveNextNode;

    return node;
}

MoveId GameX::dbAddLine(const Move::List& moveList, const QString& annotation)
{
    if(moveList.isEmpty())
    {
        return NO_MOVE;
    }
    MoveId currentPosition = currentMove();
    for(int i = 0; i < moveList.count(); ++i)
    {
        dbAddMove(moveList[i]);
        forward();
    }
    if(!annotation.isEmpty())
    {
        dbSetAnnotation(annotation);
    }
    dbMoveToId(currentPosition);
    return currentPosition;
}

MoveId GameX::dbAddVariation(const Move::List& moveList, const QString& annotation)
{
    if(moveList.isEmpty())
    {
        return NO_MOVE;
    }
    MoveId currentPosition = currentMove();
    MoveId varStart;
    int start = 1;
    if(atLineEnd() && atGameStart())
    {
        Move oldMove = move();
        backward();
        varStart = dbAddVariation(oldMove);
        start = 0;
    }
    else
    {
        varStart = dbAddVariation(moveList.first());
    }

    dbMoveToId(varStart);
    for(int i = start; i < moveList.count(); ++i)
    {
        dbAddMove(moveList[i]);
        forward();
    }
    if(!annotation.isEmpty())
    {
        dbSetAnnotation(annotation);
    }
    dbMoveToId(currentPosition);
    return varStart;
}

MoveId GameX::dbAddSanVariation(MoveId node, const QString& sanMove, const QString& annotation, NagSet nags)
{
    MoveId saveNextNode = m_moves.m_nodes[m_moves.m_currentNode].nextNode;
    MoveId newNode = dbAddSanMove(sanMove, annotation, nags);
    if(newNode != NO_MOVE)
    {
        m_moves.m_nodes[newNode].parentNode = node;
        m_moves.m_nodes[node].variations.append(newNode);
        m_moves.m_nodes[node].nextNode = saveNextNode;
    }
    return newNode;
}

void GameX::dbPromoteVariation(MoveId variation)
{
    SaveRestoreMove saveNode(*this);

    // Find first move of the variation
    while(!atLineStart(variation))
    {
        variation = m_moves.m_nodes[variation].previousNode;
    }
    MoveId parent = m_moves.m_nodes[variation].parentNode;

    // Link old main line to parent
    reparentVariation(m_moves.m_nodes[parent].nextNode, parent);
    // Link new main line to parent's parent
    reparentVariation(variation, m_moves.m_nodes[parent].parentNode);

    // Swap main line and the variation
    int index = m_moves.m_nodes[parent].variations.indexOf(variation);
    qSwap(m_moves.m_nodes[parent].nextNode, m_moves.m_nodes[parent].variations[index]);
}

bool GameX::promoteVariation(MoveId variation)
{
    if(isMainline(variation))
    {
        return false;
    }
    GameX state = *this;
    dbPromoteVariation(variation);
    emit signalGameModified(true, state, tr("Promote variation"));
    return true;
}

bool GameX::removeVariation(MoveId variation)
{
    // don't remove whole game
    if(variation == ROOT_NODE)
    {
        return false;
    }
    GameX state = *this;
    MoveId parentNode = m_moves.m_nodes[variation].parentNode;
    removeNode(variation);
    dbMoveToId(parentNode);

    QList<MoveId> &vars = m_moves.m_nodes[m_moves.m_currentNode].variations;
    vars.removeAt(vars.indexOf(variation));

    compact();
    emit signalGameModified(true, state, tr("Remove variation"));
    return true;
}

void GameX::truncateVariationAfterNextIllegalPosition()
{
    SaveRestoreMove saveNode(*this);
    QList<MoveId> variationList = variations();
    for(QList<MoveId>::iterator iter = variationList.begin(); iter != variationList.end(); ++iter)
    {
        SaveRestoreMove saveNodeLoop(*this);
        enterVariation(*iter);
        truncateVariationAfterNextIllegalPosition();
    }
    QString san = moveToSan();
    GameX g = *this;
    if(NO_MOVE == g.dbAddSanMove(san, QString(), NagSet()))
    {
        MoveId node = m_moves.m_nodes[m_moves.m_currentNode].nextNode;
        removeNode(node);
    }
    else
    {
        if(forward())
        {
            truncateVariationAfterNextIllegalPosition();
        }
    }
}

void GameX::dbTruncateVariation(Position position)
{
    if(position == AfterMove)
    {
        MoveId node = m_moves.m_nodes[m_moves.m_currentNode].nextNode;
        foreach(MoveId var, m_moves.m_nodes[m_moves.m_currentNode].variations)
        {
            removeNode(var);
        }
        removeNode(node);
    }
    else if(position == BeforeMove && m_moves.m_currentNode != 0)
    {
        if(atLineStart())
        {
            backward();
            forward();
        }
        SaveRestoreMove saveNode(*this);
        MoveNode firstNode;
        firstNode.nextNode = m_moves.m_currentNode;
        firstNode.SetPly(m_moves.m_nodes[m_moves.m_currentNode].Ply() - 1);
        // Keep variation if truncating main line
        if(m_moves.m_nodes[m_moves.m_nodes[m_moves.m_currentNode].previousNode].nextNode == m_moves.m_currentNode)
        {
            firstNode.variations = m_moves.m_nodes[m_moves.m_nodes[m_moves.m_currentNode].previousNode].variations;
            foreach(MoveId var, firstNode.variations)
            {
                reparentVariation(var, 0);
                m_moves.m_nodes[var].previousNode = 0;
            }
        }
        m_moves.m_nodes[0] = firstNode;
        m_moves.m_nodes[m_moves.m_currentNode].previousNode = 0;
        backward();
        m_moves.m_startingBoard = *m_moves.currentBoard();
        if(m_moves.m_startingBoard != BoardX::standardStartBoard)
        {
            m_tags[TagNameFEN] = m_moves.m_startingBoard.toFen();
            m_tags[TagNameSetUp] = "1";
        }
    }
    compact();
}

void GameX::truncateVariation(Position position)
{
    GameX state = *this;
    dbTruncateVariation(position);
    emit signalGameModified(true, state, tr("Truncate variation"));
}

const BoardX& GameX::board() const
{
    return *m_moves.currentBoard();
}

BoardX GameX::startingBoard() const
{
    return m_moves.m_startingBoard;
}

QString GameX::toFen() const
{
    return m_moves.currentBoard()->toFen();
}

QString GameX::toHumanFen() const
{
    return m_moves.currentBoard()->toHumanFen();
}

void GameX::setGameComment(const QString& gameComment)
{
    setAnnotation(gameComment, 0);
}

void GameX::removeVariationsDb()
{
    for(int i = 0; i < m_moves.m_nodes.size(); ++i)
    {
        while (!m_moves.m_nodes[i].variations.empty())
        {
            removeVariation(m_moves.m_nodes[i].variations.at(0));
        }
    }
    compact();
}

void GameX::removeVariations()
{
    GameX state = *this;
    removeVariationsDb();
    emit signalGameModified(true, state, tr("Remove variations"));
}

void GameX::removeCommentsDb()
{
    m_variationStartAnnotations.clear();
    m_annotations.clear();
    for(int i = 0; i < m_moves.m_nodes.size(); ++i)
    {
        m_moves.m_nodes[i].nags.clear();
    }
    compact();
}

void GameX::removeTimeCommentsFromMap(AnnotationMap& map)
{
    QRegExp tan("\\[%(egt|emt|clk)\\s*(\\d?\\d:\\d?\\d:\\d\\d)\\]");
    AnnotationMap::iterator i;
    for (i = map.begin(); i != map.end(); ++i)
    {
        QString text = i.value();
        text = text.remove(tan);
        map[i.key()] = text;
    }
}

void GameX::removeTimeCommentsDb()
{
    removeTimeCommentsFromMap(m_annotations);
}

void GameX::removeComments()
{
    GameX state = *this;
    removeCommentsDb();
    emit signalGameModified(true, state, tr("Remove comments"));
}

void GameX::removeTimeComments()
{
    GameX state = *this;
    removeTimeCommentsDb();
    emit signalGameModified(true, state, tr("Remove time comments"));
}

bool GameX::isMainline(MoveId moveId) const
{
    if(moveId == 0)
    {
        return true;
    }
    MoveId node = m_moves.makeNodeIndex(moveId);
    if(node == NO_MOVE)
    {
        return false;
    }
    else
    {
        return m_moves.m_nodes[node].parentNode == NO_MOVE;
    }
}

Result GameX::result() const
{
    if(m_tags[TagNameResult] == "1-0")
    {
        return WhiteWin;
    }
    else if(m_tags[TagNameResult] == "1/2-1/2")
    {
        return Draw;
    }
    else if(m_tags[TagNameResult] == "0-1")
    {
        return BlackWin;
    }
    else
    {
        return ResultUnknown;
    }
}

int GameX::resultAsInt() const
{
    if(m_tags[TagNameResult] == "1-0")
    {
        return +1;
    }
    else if(m_tags[TagNameResult] == "1/2-1/2")
    {
        return 0;
    }
    else if(m_tags[TagNameResult] == "0-1")
    {
        return -1;
    }
    else
    {
        return 0;
    }
}

int GameX::numberOfSiblings(MoveId moveId) const
{
    MoveId node = m_moves.makeNodeIndex(moveId);
    if(node == NO_MOVE)
    {
        return 0;
    }
    MoveId parentNode = m_moves.m_nodes[node].parentNode;
    if(parentNode == NO_MOVE)
    {
        return 0;
    }
    return m_moves.m_nodes[parentNode].variations.size();
}

bool GameX::atLineStart(MoveId moveId) const
{
    MoveId node = m_moves.makeNodeIndex(moveId);
    if(node == NO_MOVE)
    {
        return false;
    }
    return (m_moves.m_nodes[node].previousNode == m_moves.m_nodes[node].parentNode)
        || m_moves.m_nodes[node].previousNode == 0;
}

bool GameX::atGameStart(MoveId moveId) const
{
    return (m_moves.makeNodeIndex(moveId) == 0);
}

bool GameX::atGameEnd(MoveId moveId) const
{
    return (atLineEnd(moveId) && isMainline(moveId));
}

bool GameX::atLineEnd(MoveId moveId) const
{
    MoveId node = m_moves.makeNodeIndex(moveId);
    if(node != NO_MOVE)
    {
        if(m_moves.m_nodes[node].nextNode == NO_MOVE)
        {
            return true;
        }
    }

    return false;
}

bool GameX::setAnnotation(QString annotation, MoveId moveId, Position position)
{
    GameX state = *this;
    if (dbSetAnnotation(annotation, moveId, position))
    {
        dbIndicateAnnotationsOnBoard(currentMove());
        emit signalGameModified(true, state, tr("Set annotation"));
        return true;
    }
    return false;
}

bool GameX::editAnnotation(QString annotation, MoveId moveId, Position position)
{
    GameX state = *this;
    QString spec = specAnnotations(moveId);
    annotation.append(spec);
    if (dbSetAnnotation(annotation, moveId, position))
    {
        dbIndicateAnnotationsOnBoard(currentMove());
        emit signalGameModified(true, state, "Edit annotation");
        return true;
    }
    return false;
}

bool GameX::dbAppendAnnotation(QString a, MoveId moveId, Position position)
{
    QString s = annotation();
    s.append(a);
    return dbSetAnnotation(s, moveId, position);
}

bool GameX::dbPrependAnnotation(QString a, MoveId moveId, Position position)
{
    QString s = annotation();
    s.prepend(a);
    return dbSetAnnotation(s, moveId, position);
}

bool GameX::dbSetAnnotation(QString annotation, MoveId moveId, Position position)
{
    MoveId node = m_moves.makeNodeIndex(moveId);
    if(node == NO_MOVE)
    {
        return false;
    }

    int moves;
    int comments;
    int nags;
    moveCount(&moves, &comments, &nags);

    annotation.remove('}'); // Just make sure a comment does not destroy the database

    if(position == AfterMove)
    {
        if(annotation.isEmpty())
        {
            m_annotations.remove(node);
        }
        else
        {
            m_annotations[node] = annotation;
        }
    }
    else if(canHaveStartAnnotation(node))  	// Pre-move comment
    {
        if (node == 0)
        {
            setGameComment(annotation);
        }
        else
        {
            if(annotation.isEmpty() && (node > 1)) // Do not remove empty comment
            {
                m_variationStartAnnotations.remove(node);
            }
            else
            {
                m_variationStartAnnotations[node] = annotation;
            }
        }
    }
    return true;
}

bool GameX::setSquareAnnotation(QString squareAnnotation, MoveId moveId)
{
    squareAnnotation = squareAnnotation.trimmed();

    MoveId node = m_moves.makeNodeIndex(moveId);
    if(node == NO_MOVE)
    {
        return false;
    }

    QString s = annotation(moveId);
    s.remove(QRegExp(s_csl));

    if(!squareAnnotation.isEmpty())
    {
        s.append(QString("[%csl %1]").arg(squareAnnotation));
    }
    dbSetAnnotation(s, moveId);
    indicateAnnotationsOnBoard(moveId);
    return true;
}

bool GameX::appendSquareAnnotation(Square s, QChar colorCode)
{
    GameX state = *this;
    QString newAnnot;
    QString annot = squareAnnotation();
    QString oldAnnot = annot;
    QString sq = strSquareNames[s];
    if(annot.isEmpty())
    {
        if(colorCode != QChar(0))
        {
            newAnnot = QString("%1%2").arg(colorCode).arg(sq);
        }
    }
    else
    {
        annot.replace(QRegExp(QString(",.") + sq), ""); 	// if not first annotation remove annotation with preceding comma
        annot.replace(QRegExp(QString(".") + sq + ","), ""); // if first annotation remove annotation with trailing comma
        annot.replace(QRegExp(QString(".") + sq), ""); 	// if only annotation remove annotation
        if(colorCode != QChar(0))
        {
            if(annot.isEmpty())
            {
                newAnnot = QString("%1%2").arg(colorCode).arg(sq);	// If only annotation don't add comma
            }
            else
            {
                newAnnot = QString("%1,%2%3").arg(annot).arg(colorCode).arg(sq);	// if not only annotation add comma
            }
        }
        else
        {
            if(!annot.isEmpty())
            {
                newAnnot = annot;
            }
        }
        newAnnot.replace(" ,", " ");
        newAnnot.replace(",,", ",");
        if(newAnnot.endsWith("'"))
        {
            newAnnot.truncate(newAnnot.length() - 1);
        }
    }
    setSquareAnnotation(newAnnot.trimmed());
    emit signalGameModified(true, state, tr("Colorize square"));
    return true;
}

bool GameX::appendArrowAnnotation(Square dest, Square src, QChar colorCode)
{
    if((src == InvalidSquare) || (dest == InvalidSquare))
    {
        return false;
    }
    GameX state = *this;
    QString newAnnot;
    QString annot = arrowAnnotation();
    QString sqSrc = strSquareNames[src];
    QString sqDest = strSquareNames[dest];
    if(annot.isEmpty())
    {
        if(colorCode != QChar(0))
        {
            newAnnot = QString("%1%2%3").arg(colorCode).arg(sqSrc).arg(sqDest);
        }
    }
    else
    {
        annot.replace(QRegExp(QString(",.") + sqSrc + sqDest), ""); 	// if not first annotation remove annotation with preceding comma
        annot.replace(QRegExp(QString(".") + sqSrc + sqDest + ","), ""); // if first annotation remove annotation with trailing comma
        annot.replace(QRegExp(QString(".") + sqSrc + sqDest), ""); 	// if only annotation remove annotation

        if(colorCode != QChar(0))
        {
            newAnnot = QString("%1,%2%3%4").arg(annot).arg(colorCode).arg(sqSrc).arg(sqDest);
        }
        else
        {
            if(!annot.isEmpty())
            {
                newAnnot = annot;
            }
        }
        newAnnot.replace(" ,", " ");
        newAnnot.replace(",,", ",");
        if(newAnnot.endsWith("'"))
        {
            newAnnot.truncate(newAnnot.length() - 1);
        }
    }
    setArrowAnnotation(newAnnot);
    emit signalGameModified(true, state, tr("Paint arrow"));
    return true;
}

QString GameX::squareAnnotation(MoveId moveId) const
{
    QString s = specAnnotation(QRegExp(s_csl), moveId);
    return s;
}

bool GameX::setArrowAnnotation(QString arrowAnnotation, MoveId moveId)
{
    arrowAnnotation = arrowAnnotation.trimmed();

    MoveId node = m_moves.makeNodeIndex(moveId);
    if(node == NO_MOVE)
    {
        return false;
    }

    QString s = annotation(moveId);
    s.remove(QRegExp(s_cal));

    if(!arrowAnnotation.isEmpty())
    {
        s.append(QString("[%cal %1]").arg(arrowAnnotation));
    }
    dbSetAnnotation(s, moveId);
    indicateAnnotationsOnBoard(moveId);
    return true;
}

QString GameX::arrowAnnotation(MoveId moveId) const
{
    QString s = specAnnotation(QRegExp(s_cal), moveId);
    return s;
}

QString GameX::specAnnotation(const QRegExp& r, MoveId moveId) const
{
    MoveId node = m_moves.makeNodeIndex(moveId);
    if(node == NO_MOVE)
    {
        return QString("");
    }

    QString annotation = m_annotations[node];
    if(annotation.isNull())
    {
        return QString("");
    }

    int pos = r.indexIn(annotation);
    if(pos >= 0)
    {
        return r.cap(2);
    }

    return "";
}

QString GameX::timeAnnotation(MoveId moveId, Position position) const
{
    if (position == BeforeMove)
    {
        MoveId node = m_moves.makeNodeIndex(moveId);
        if (node>ROOT_NODE)
        {
            moveId = m_moves.m_nodes.at(node).previousNode;
        }
        else return "";
    }

    QString s = specAnnotation(QRegExp(s_tan), moveId);
    s = s.trimmed();
    return s;
}

QString GameX::annotation(MoveId moveId, Position position) const
{
    MoveId node = m_moves.makeNodeIndex(moveId);

    if ((position == AfterMove) || (node == 0))
    {
        return m_annotations.value(node,QString(""));
    }
    else
    {
        return m_variationStartAnnotations.value(node,QString(""));
    }
}

QString GameX::specAnnotations(MoveId moveId, Position position) const
{
    QString s = annotation(moveId, position);
    QString retval;
    foreach (QString sr, s_specList)
    {
        QRegExp r(sr);
        int pos = r.indexIn(s);
        if(pos >= 0)
        {
            retval += r.cap(0);
        }
    }
    return retval;
}

QString GameX::textAnnotation(MoveId moveId, Position position) const
{
    QString s = annotation(moveId, position);
    if (!s.isEmpty())
    {
        s.remove(QRegExp(s_tan));
        s.remove(QRegExp(s_can));
    }
    return s;
}

bool GameX::canHaveStartAnnotation(MoveId moveId) const
{
    MoveId node = m_moves.makeNodeIndex(moveId);
    return atLineStart(moveId) || atGameStart(m_moves.m_nodes[node].previousNode);
}

bool GameX::dbAddNag(Nag nag, MoveId moveId)
{
    MoveId node = m_moves.makeNodeIndex(moveId);
    if ((node != NO_MOVE) && (nag != NullNag))
    {
        m_moves.m_nodes[node].nags.addNag(nag);
        return true;
    }
    return false;
}

bool GameX::addNag(Nag nag, MoveId moveId)
{
    MoveId node = m_moves.makeNodeIndex(moveId);
    if ((node != NO_MOVE) && (nag != NullNag))
    {
        GameX state = *this;
        dbAddNag(nag,moveId);
        emit signalGameModified(true, state, tr("Add nag"));
        return true;
    }
    return false;
}

bool GameX::setNags(NagSet nags, MoveId moveId)
{
    MoveId node = m_moves.makeNodeIndex(moveId);
    if(node != NO_MOVE)
    {
        GameX state = *this;
        m_moves.m_nodes[node].nags = nags;
        emit signalGameModified(true, state, tr("Set nags"));
        return true;
    }
    return false;
}

bool GameX::clearNags(MoveId moveId)
{
    return setNags(NagSet(), moveId);
}

NagSet GameX::nags(MoveId moveId) const
{
    MoveId node = m_moves.makeNodeIndex(moveId);
    if(node != NO_MOVE)
    {
        return m_moves.m_nodes[node].nags;
    }
    return NagSet();
}

void GameX::moveCount(int* moves, int* comments, int* nags) const
{
    *moves = *comments = 0;
    if (nags) *nags = 0;

    MoveId node = 1;
    while((node = m_moves.makeNodeIndex(node)) != NO_MOVE)
    {
        *moves += 1;
        if (nags)
        {
            if(m_moves.m_nodes[node].nags.count() != 0)
            {
                *nags += 1;
            }
        }
        node = m_moves.m_nodes[node].nextNode;
    }
    // Count comments
    for(int i = 0; i < m_annotations.size(); ++i)
    {
        *comments += 1;
    }
    for(int i = 0; i < m_variationStartAnnotations.size(); ++i)
    {
        *comments += 1;
    }
}

bool GameX::isEmpty() const
{
    int moves;
    int comments;
    moveCount(&moves, &comments);
    bool gameIsEmpty = ((moves+comments) == 0);
    return gameIsEmpty;
}

int GameX::ply(MoveId moveId) const
{
    MoveId node = m_moves.makeNodeIndex(moveId);
    if(node != NO_MOVE)
    {
        return m_moves.m_nodes[node].Ply();
    }
    return 0;
}

int GameX::moveNumber(MoveId moveId) const
{
    MoveId node = m_moves.makeNodeIndex(moveId);
    if(node != NO_MOVE)
    {
        if(int plyNum = ply(node))
        {
            return (m_moves.m_startPly + plyNum - 1) / 2 + 1;
        }
        return 0;
    }
    return -1;
}

int GameX::plyCount() const
{
    int count = 0;
    MoveId node = 0;

    while(node != NO_MOVE)
    {
        ++count;
        node = m_moves.m_nodes[node].nextNode;
    }
    // Counted one too much, because we have to start at zero
    // (an empty game has no node 1)
    return count - 1;
}

bool GameX::canMoveVariationUp(MoveId moveId) const
{
    if(isMainline())
    {
        return false;
    }

    MoveId variation = variationNumber(moveId);
    MoveId parentNode = m_moves.m_nodes[moveId].parentNode;

    const QList <MoveId>& v = m_moves.m_nodes[parentNode].variations;
    int i = v.indexOf(variation);
    return (i > 0);
}

void GameX::moveVariationUp(MoveId moveId)
{
    if(isMainline())
    {
        return;
    }

    MoveId variation = variationNumber(moveId);
    MoveId parentNode = m_moves.m_nodes[moveId].parentNode;

    QList <MoveId>& v = m_moves.m_nodes[parentNode].variations;
    int i = v.indexOf(variation);
    if(i > 0)
    {
        GameX state = *this;
        v.swap(i, i - 1);
        emit signalGameModified(true, state, tr("Move variation"));
    }
}

bool GameX::canMoveVariationDown(MoveId moveId) const
{
    if(isMainline())
    {
        return false;
    }

    MoveId variation = variationNumber(moveId);
    MoveId parentNode = m_moves.m_nodes[moveId].parentNode;

    const QList <MoveId>& v = m_moves.m_nodes[parentNode].variations;
    int i = v.indexOf(variation);
    return (i >= 0 && (i + 1) < v.count());
}

void GameX::moveVariationDown(MoveId moveId)
{
    if(isMainline())
    {
        return;
    }

    MoveId variation = variationNumber(moveId);
    MoveId parentNode = m_moves.m_nodes[moveId].parentNode;

    QList <MoveId>& v = m_moves.m_nodes[parentNode].variations;
    int i = v.indexOf(variation);
    if(i >= 0 && (i + 1) < v.count())
    {
        GameX state = *this;
        v.swap(i, i + 1);
        emit signalGameModified(true, state, tr("Move variation"));
    }
}

void GameX::enumerateVariations(MoveId moveId, char a)
{
    if(isMainline())
    {
        return;
    }

    MoveId node = m_moves.makeNodeIndex(moveId);
    if(node != NO_MOVE)
    {
        GameX state = *this;
        MoveId parentNode = m_moves.m_nodes[node].parentNode;
        QList <MoveId>& v = m_moves.m_nodes[parentNode].variations;
        if (!v.empty())
        {
            for(int i = 0; i < v.size(); ++i)
            {
                QString oldAnnotation = annotation(v[i], GameX::BeforeMove);
                oldAnnotation.remove(QRegExp("^.\\)"));
                QString s = QString("%1) %2").arg(QChar(a + i)).arg(oldAnnotation).trimmed();
                dbSetAnnotation(s, v[i], GameX::BeforeMove);
            }
            emit signalGameModified(true, state, tr("Enumerate variations"));
        }
    }
}

MoveId GameX::variationNumber(MoveId moveId) const
{
    if(isMainline())
    {
        return 0;
    }
    MoveId node = m_moves.makeNodeIndex(moveId);
    if(node != NO_MOVE)
    {
        MoveId parentNode = m_moves.m_nodes[node].parentNode;

        while(m_moves.m_nodes[node].previousNode != parentNode)
        {
            node = m_moves.m_nodes[node].previousNode;
        }
    }
    return node;
}

MoveId GameX::mainLineMove() const
{
    MoveId node = m_moves.makeNodeIndex(m_moves.m_currentNode);
    if(node != NO_MOVE)
    {
        bool dive = false;
        while(m_moves.m_nodes[node].parentNode != NO_MOVE)
        {
            dive = true;
            node = m_moves.m_nodes[node].parentNode;
        }
        if(dive)
        {
            node = m_moves.m_nodes[node].nextNode;
        }
    }
    return node;
}

MoveId GameX::variationStartMove(MoveId variation) const
{
    variation = m_moves.makeNodeIndex(variation);
    if(variation == NO_MOVE)
    {
        return NO_MOVE;
    }
    if(isMainline(variation))
    {
        return NO_MOVE;
    }
    while(!atLineStart(variation))
    {
        variation = m_moves.m_nodes[variation].previousNode;
    }
    return variation;
}

MoveId GameX::previousMove() const
{
    return m_moves.m_nodes[m_moves.m_currentNode].previousNode;
}

MoveId GameX::nextMove() const
{
    return m_moves.m_nodes[m_moves.m_currentNode].nextNode;
}

MoveId GameX::parentMove() const
{
    return m_moves.m_nodes[m_moves.m_currentNode].parentNode;
}

MoveId GameX::currentMove() const
{
    return m_moves.m_currentNode;
}

MoveId GameX::lastMove() const
{
    MoveId moveId = 0;
    while((m_moves.m_nodes[moveId].nextNode != NO_MOVE))
    {
        moveId = m_moves.m_nodes[moveId].nextNode;
    }
    return moveId;
}

const QList<MoveId>& GameX::variations() const
{
    return m_moves.m_nodes[m_moves.m_currentNode].variations;
}

int GameX::variationCount(MoveId moveId) const
{
    MoveId node = m_moves.makeNodeIndex(moveId);
    if(node != NO_MOVE)
    {
        return m_moves.m_nodes[node].variations.size();
    }
    return 0;
}

bool GameX::variationHasSiblings(MoveId variation) const
{
    variation = m_moves.makeNodeIndex(variation);
    if(variation == NO_MOVE)
    {
        return false;
    }
    if(isMainline(variation))
    {
        return false;
    }
    while(!atLineStart(variation))
    {
        variation = m_moves.m_nodes[variation].previousNode;
    }
    MoveId parent = m_moves.m_nodes[variation].parentNode;
    return (variationCount(parent) > 1);
}

void GameX::dbIndicateAnnotationsOnBoard(MoveId moveId)
{
    QString annotation = squareAnnotation(moveId);
    m_moves.currentBoard()->setSquareAnnotation(annotation);

    annotation = arrowAnnotation(moveId);
    m_moves.currentBoard()->setArrowAnnotation(annotation);
}

void GameX::indicateAnnotationsOnBoard(MoveId moveId)
{
    dbIndicateAnnotationsOnBoard(moveId);
    emit signalMoveChanged();
}

void GameX::moveToStart()
{
    m_moves.m_currentNode = 0;
    *m_moves.currentBoard() = m_moves.m_startingBoard;

    indicateAnnotationsOnBoard(m_moves.m_currentNode);
}

int GameX::moveByPly(int diff)
{
    if(diff > 0)
    {
        return forward(diff);
    }
    else
    {
        return backward(-diff);
    }
}

bool GameX::dbMoveToId(MoveId moveId, QString* algebraicMoveList)
{
    moveId = m_moves.makeNodeIndex(moveId);
    if(moveId == NO_MOVE)
    {
        return false;
    }

    if (m_moves.m_currentNode != moveId)
    {
        //jump to node, travelling back to start adding the moves to the stack
        MoveId node = moveId;
        QStack<Move> moveStack;
        while(node)
        {
            moveStack.push(m_moves.m_nodes[node].move);
            node = m_moves.m_nodes[node].previousNode;
        }

        //reset the board, then make the moves on the stack to create the correct position
        m_moves.m_currentNode = moveId;
        *m_moves.currentBoard() = m_moves.m_startingBoard;
        while(!moveStack.isEmpty())
        {
            Move m = moveStack.pop();
            m_moves.currentBoard()->doMove(m);
            if (algebraicMoveList)
            {
                if (m.isNullMove())
                {
                    // Avoid trouble with a null move - UCI does not specify this and Stockfish makes nonsense
                    algebraicMoveList->clear();
                    algebraicMoveList = nullptr;
                }
                else
                {
                    algebraicMoveList->push_back(m.toAlgebraic());
                    algebraicMoveList->push_back(" ");
                }
            }
        }
    }

    return true;
}

void GameX::moveToId(MoveId moveId)
{
    if (dbMoveToId(moveId))
    {
        indicateAnnotationsOnBoard(moveId);
    }
}

Move GameX::move(MoveId moveId) const
{
    MoveId node = m_moves.makeNodeIndex(moveId);
    if(node != NO_MOVE)
    {
        return m_moves.m_nodes[node].move;
    }
    return Move();
}

bool GameX::dbMoveToLineEnd()
{
    return (!forward(999));
}

bool GameX::dbMoveToEnd()
{
    // Move out of variations to mainline
    while(m_moves.m_nodes[m_moves.m_currentNode].parentNode != NO_MOVE)
    {
        dbMoveToId(m_moves.m_nodes[m_moves.m_currentNode].parentNode);
    }
    // Now move forward to the end of the game
    return dbMoveToLineEnd();
}

void GameX::moveToEnd()
{
    if (!dbMoveToEnd())
    {
        indicateAnnotationsOnBoard(m_moves.m_currentNode);
    }
}

void GameX::moveToLineEnd()
{
    if (!dbMoveToEnd())
    {
        indicateAnnotationsOnBoard(m_moves.m_currentNode);
    }
}

int GameX::dbForward(int count)
{
    int moved = 0;
    while ((m_moves.m_nodes[m_moves.m_currentNode].nextNode != NO_MOVE) && (moved < count))
    {
        m_moves.m_currentNode = m_moves.m_nodes[m_moves.m_currentNode].nextNode;
        ++moved;

        m_moves.currentBoard()->doMove(m_moves.m_nodes[m_moves.m_currentNode].move);
    }
    return moved;
}

int GameX::forward(int count)
{
    int moved = dbForward(count);
    if (moved)
    {
        indicateAnnotationsOnBoard(m_moves.m_currentNode);
    }
    return moved;
}

int GameX::dbBackward(int count)
{
    int moved = 0;
    while((m_moves.m_nodes[m_moves.m_currentNode].previousNode >= 0) && (moved < count))
    {
        m_moves.currentBoard()->undoMove(m_moves.m_nodes[m_moves.m_currentNode].move);
        m_moves.m_currentNode = m_moves.m_nodes[m_moves.m_currentNode].previousNode;
        ++moved;
    }
    return moved;
}

int GameX::backward(int count)
{
    int moved = dbBackward(count);
    if(moved)
    {
        indicateAnnotationsOnBoard(m_moves.m_currentNode);
    }
    return moved;
}

void GameX::enterVariation(const MoveId& moveId)
{
    Q_ASSERT(variations().contains(moveId));
    m_moves.currentBoard()->doMove(m_moves.m_nodes[moveId].move);
    m_moves.m_currentNode = moveId;

    indicateAnnotationsOnBoard(m_moves.m_currentNode);
}

void GameX::removeNode(MoveId moveId)
{
    MoveId node = m_moves.makeNodeIndex(moveId);
    if(node > ROOT_NODE)
    {
        m_variationStartAnnotations.remove(node);
        m_annotations.remove(node);

        for(int i = 0; i < m_moves.m_nodes[node].variations.size(); ++i)
        {
            removeNode(m_moves.m_nodes[node].variations[i]);
        }
        removeNode(m_moves.m_nodes[node].nextNode);
        MoveId prevNode = m_moves.m_nodes[node].previousNode;
        MoveId parentNode = m_moves.m_nodes[node].parentNode;
        if ((prevNode >= ROOT_NODE) && (parentNode != prevNode))
        {
            // Do not delete main variation above sub variation
            m_moves.m_nodes[prevNode].nextNode = NO_MOVE;
        }
        m_moves.m_nodes[node].remove();
    }
}

void GameX::clear()
{
    m_moves.m_nodes.clear();
    m_variationStartAnnotations.clear();
    m_annotations.clear();

    m_moves.m_startPly = 0;

    m_moves.m_startingBoard.setStandardPosition();
    if (m_moves.currentBoard())
    {
        m_moves.m_currentNode = 0;
        *m_moves.currentBoard() = m_moves.m_startingBoard;
    }
    else
    {
        m_moves.m_currentNode = NO_MOVE;
    }

    m_moves.m_nodes.append(MoveNode());
}

void GameX::clearTags()
{
    m_tags.clear();
}

QString GameX::tag(const QString& tag) const
{
    return m_tags[tag];
}

bool GameX::hasTag(const QString& tag) const
{
    return m_tags.contains(tag);
}

const TagMap& GameX::tags() const
{
    return m_tags;
}

void GameX::setTag(const QString& tag, const QString& value)
{
    m_tags[tag] = value;
}

void GameX::removeTag(const QString& tag)
{
    m_tags.remove(tag);
}

void GameX::setStartingBoard(const BoardX& startingBoard, QString text, bool chess960)
{
    GameX state = *this;
    dbSetStartingBoard(startingBoard.toFen(), chess960);
    emit signalGameModified(true, state, text);
}

void GameX::dbSetStartingBoard(const QString& fen, bool chess960)
{
    clear();
    dbSetChess960(chess960);
    m_moves.m_startingBoard.setChess960(chess960);
    m_moves.m_startingBoard.fromFen(fen);
    if(m_moves.m_startingBoard != BoardX::standardStartBoard)
    {
        m_tags[TagNameFEN] = fen;
        m_tags[TagNameSetUp] = "1";
    }
    if (m_moves.currentBoard())
    {
        *m_moves.currentBoard() = m_moves.m_startingBoard;
    }
    m_moves.m_startPly = (m_moves.m_startingBoard.moveNumber() - 1) * 2 + (m_moves.m_startingBoard.toMove() == Black);
}

void GameX::dbSetResult(Result result)
{
    m_tags[TagNameResult] = resultString(result);
}

void GameX::setResult(Result result)
{
    GameX state = *this;
    dbSetResult(result);
    emit signalGameModified(true, state, tr("Set result"));
}

bool GameX::isChess960() const
{
    QString s = m_tags[TagNameVariant].toLower();
    return (s.startsWith("fischer") || s.endsWith("960"));
}

void GameX::dbSetChess960(bool b)
{
    if (b)
    {
        m_tags[TagNameVariant] = "Chess960";
    }
    else
    {
        removeTag(TagNameVariant);
    }
}

void GameX::setChess960(bool b)
{
    GameX state = *this;
    dbSetChess960(b);
    emit signalGameModified(true, state, tr("Set variant"));
}

QString GameX::moveToSan(MoveStringFlags flags, NextPreviousMove nextPrevious, MoveId moveId,
                         QString* annots, NagSet* nagSet)
{
    MoveId node = m_moves.makeNodeIndex(moveId);
    if(node != NO_MOVE && nextPrevious == NextMove)
    {
        node = m_moves.m_nodes[node].nextNode;
    }
    if(node == NO_MOVE)
    {
        return QString();
    }

    MoveNode move;
    move = m_moves.m_nodes[node];
    if(!(move.move.isLegal() || move.move.isNullMove()))
    {
        return QString();
    }

    // Save current node
    MoveId saveNode = NO_MOVE;
    MoveId boardNode = m_moves.m_nodes[node].previousNode;
    if(boardNode != m_moves.m_currentNode)
    {
        saveNode = m_moves.m_currentNode;
        dbMoveToId(boardNode);
    }

    // Move number
    QString san;
    if(m_moves.currentBoard()->toMove() == Black && flags & BlackNumbers)
    {
        san += QString::number(moveNumber(node)) + "... ";
    }
    else if(m_moves.currentBoard()->toMove() == White && flags & WhiteNumbers)
    {
        san += QString::number(moveNumber(node)) + ". ";
    }

    // Move and SAN
    san += m_moves.currentBoard()->moveToSan(move.move, flags & TranslatePiece);
    if(flags & Nags)
    {
        san += nags(node).toString(NagSet::Simple);
    }

    if(nagSet)
    {
        *nagSet = nags(node);
    }
    if(annots)
    {
        *annots = annotation(node);
    }

    // Restore previous position
    if(saveNode != NO_MOVE)
    {
        dbMoveToId(saveNode);
    }

    return san;
}

void GameX::dumpMoveNode(MoveId moveId) const
{
    if(moveId == CURRENT_MOVE)
    {
        moveId = m_moves.m_currentNode;
    }
    if(moveId != NO_MOVE)
    {
        qDebug() << "Move Id : " << moveId;
        qDebug() << "   Next node   : " << m_moves.m_nodes.at(moveId).nextNode;
        qDebug() << "   Prev node   : " << m_moves.m_nodes.at(moveId).previousNode;
        qDebug() << "   Parent node : " << m_moves.m_nodes.at(moveId).parentNode;
        qDebug() << "   Nags        : " << m_moves.m_nodes.at(moveId).nags.toString(NagSet::Simple);
        qDebug() << "   Deleted     : " << m_moves.m_nodes.at(moveId).Removed();
        qDebug() << "   # Variations: " << m_moves.m_nodes.at(moveId).variations.size();
        qDebug() << "   Variations  : " << m_moves.m_nodes.at(moveId).variations;
        qDebug() << "   Move        : " << m_moves.m_nodes.at(moveId).move.toAlgebraic()
                 << " (" << m_moves.m_nodes.at(moveId).move.rawMove()
                 << ", " << m_moves.m_nodes.at(moveId).move.rawUndo()
                 << ")";
    }
}

void GameX::dumpAnnotations(MoveId moveId) const
{
    if(moveId == CURRENT_MOVE)
    {
        moveId = m_moves.m_currentNode;
    }
    if(moveId != NO_MOVE)
    {
        qDebug() << "   Annotations";
        qDebug() << "     Text        : " << m_annotations.value(moveId);
        qDebug() << "     Start       : " << m_variationStartAnnotations.value(moveId);
    }
}


void GameX::dumpAllMoveNodes() const
{
    qDebug() << endl;
    qDebug() << "Current Node: " << m_moves.m_currentNode;
    for(int i = 0; i < m_moves.m_nodes.size(); ++i)
    {
        dumpMoveNode(i);
        dumpAnnotations(i);
    }
    int moves, comments, nags;
    moveCount(&moves, &comments, &nags);
    qDebug() << "Moves: " << moves << " Comments: " << comments << " Nags: " << nags << endl;
    qDebug() << "----------------------------------" << endl;
}

MoveId GameX::findPosition(const BoardX& position) const
{
    MoveId current = 0;
    BoardX currentBoard(m_moves.m_startingBoard);

    for(;;)
    {
        if(currentBoard == position && currentBoard.positionIsSame(position))
        {
            return current;
        }

        current = m_moves.m_nodes[current].nextNode;
        if(current == NO_MOVE || !position.canBeReachedFrom(currentBoard))
        {
            return NO_MOVE;
        }

        currentBoard.doMove(m_moves.m_nodes[current].move);
    }
    return NO_MOVE;
}

void GameX::compact()
{
    int oldSize = m_moves.m_nodes.size();
    QList<MoveNode> moveNodes;
    QMap<MoveId,MoveId> oldIdNewIdMapping;
    QList<MoveId> removedNodes;

    for(MoveId i = 0; i < oldSize; ++i)
    {
        if(!m_moves.m_nodes[i].Removed())
        {
            oldIdNewIdMapping[i] = moveNodes.size();
            moveNodes.append(m_moves.m_nodes[i]);
        }
        else
        {
            removedNodes.push_back(i);
        }
    }

    foreach(MoveId m, removedNodes)
    {
        m_variationStartAnnotations.remove(m);
        m_annotations.remove(m);
    }

    AnnotationMap variationStartAnnotations;
    AnnotationMap annotations;

    foreach(MoveId key, oldIdNewIdMapping.keys())
    {
        MoveId n = oldIdNewIdMapping.value(key);
#define GAME_UPDATE_ANNOT(t,x) \
        if (x.contains(key)) \
        {\
            QString s = x.value(key);\
            t[n] = s;\
        }

        GAME_UPDATE_ANNOT(variationStartAnnotations, m_variationStartAnnotations)
        GAME_UPDATE_ANNOT(annotations, m_annotations)
    }

    // update nodes links to other nodes in shrinked list (prev, next, variations)
    for(MoveId i = 0, newSize = moveNodes.size(); i < newSize; ++i)
    {
        MoveNode& node = moveNodes[i];
#define GAME_UPDATE_MOVEID(aMoveId) if (aMoveId != NO_MOVE) aMoveId = oldIdNewIdMapping[aMoveId]
        GAME_UPDATE_MOVEID(node.nextNode);
        GAME_UPDATE_MOVEID(node.previousNode);
        GAME_UPDATE_MOVEID(node.parentNode);
        QList<MoveId>& vars = node.variations;
        for(int j = 0; j < vars.size(); ++j)
        {
            GAME_UPDATE_MOVEID(vars[j]);
        }
        vars.removeAll(NO_MOVE);
        vars.removeAll(0);
#undef GAME_UPDATE_MOVEID
    }

    m_variationStartAnnotations.clear();
    m_annotations.clear();

    m_variationStartAnnotations = variationStartAnnotations;
    m_annotations               = annotations;

    //m_variationStartAnnotations.detach();
    //m_annotations.detach();

    m_moves.m_nodes.clear();
    m_moves.m_nodes = moveNodes;
    m_moves.m_currentNode = oldIdNewIdMapping[m_moves.m_currentNode];
}

QString GameX::ecoClassify() const
{
    //move to end of main line
    GameX g = *this;
    if (g.startingBoard() != BoardX::standardStartBoard)
    {
        if (g.isChess960())
        {
            return QString();
        }
    }
    g.dbMoveToEnd();

    //search backwards for the first eco position
    while(g.dbBackward())
    {
        QString eco;
        if (EcoPositions::isEcoPosition(g.board(),eco))
        {
            return eco;
        }
    }

    return QString();
}

bool GameX::isEcoPosition() const
{
    QString dummy;
    return (EcoPositions::isEcoPosition(board(),dummy));
}

void GameX::scoreMaterial(QList<double>& scores) const
{
    GameX g = *this;
    g.moveToStart();
    scores.clear();
    int score = g.board().ScoreMaterial();
    scores.append(score);
    while(g.forward())
    {
        int score = g.board().ScoreMaterial();
        scores.append(score);
    }
}

void GameX::reparentVariation(MoveId variation, MoveId parent)
{
    if(variation != NO_MOVE)
    {
        for(MoveId node = variation; node != NO_MOVE; node = m_moves.m_nodes[node].nextNode)
        {
            m_moves.m_nodes[node].parentNode = parent;
        }
    }
}

int GameX::isEqual(const GameX& game) const
{
    return ((m_moves.m_nodes == game.m_moves.m_nodes) &&
            (m_annotations == game.m_annotations) &&
            (m_variationStartAnnotations == game.m_variationStartAnnotations));
}

int GameX::isBetterOrEqual(const GameX& game) const
{
    return ((m_moves.m_nodes.count() >= game.m_moves.m_nodes.count()) &&
            (m_annotations.count() >= game.m_annotations.count()) &&
            (m_variationStartAnnotations.count() >= game.m_variationStartAnnotations.count()));
}
